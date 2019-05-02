-- Copyright (c) 2018 Patrick Fay
--
-- License http://opensource.org/licenses/mit-license.php MIT License

function got_problems( error_msg )
	print(debug.traceback())
	return "got_problems handler: " .. error_msg
end

printf = function(s,...)
           return io.write(s:format(...))
end -- function

function ParseCSVLine (line,sep) 
	-- from http://lua-users.org/wiki/LuaCsv
	local res = {}
	local pos = 1
	sep = sep or ','
	while true do 
		local c = string.sub(line,pos,pos)
		if (c == "") then break end
		if (c == '"') then
			-- quoted value (ignore separator within)
			local txt = ""
			repeat
				local startp,endp = string.find(line,'^%b""',pos)
				txt = txt..string.sub(line,startp+1,endp-1)
				pos = endp + 1
				c = string.sub(line,pos,pos) 
				if (c == '"') then txt = txt..'"' end 
				-- check first char AFTER quoted string, if it is another
				-- quoted string without separator, then append it
				-- this is the way to "escape" the quote char in a quote. example:
				--   value1,"blub""blip""boing",value3  will result in blub"blip"boing  for the middle
			until (c ~= '"')
			table.insert(res,txt)
			assert(c == sep or c == "")
			pos = pos + 1
		else	
			-- no quotes used, just look for the first separator
			local startp,endp = string.find(line,sep,pos)
			if (startp) then 
				table.insert(res,string.sub(line,pos,startp-1))
				pos = endp + 1
			else
				-- no separator found -> use rest of string and terminate
				table.insert(res,string.sub(line,pos))
				break
			end 
		end
	end
	return res
end

function mysplit(inputstr, sep)
        if sep == nil then
                sep = "%s"
        end
        local t= {} ; i=1
        for str in string.gmatch(inputstr, "([^"..sep.."]+)") do
                t[i] = str
                i = i + 1
        end
        return t
end

function dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..k..'"' end
         s = s .. '['..k..'] = ' .. dump(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end

data_table = {}
data_shape = {}
col = {}
evts = 0
evt_hash = {}
evt_units_hash = {}

function read_file(flnm, hdr, data, verbose)
   local rows = 0
   local row = -1
   for line in io.lines(flnm) do
	   local b, e = string.find(line, "beg/end")
	   if b ~= nil then
			if verbose > 0 then
		   		printf("line[%d]= %s\n", rows, line)
			end
		local splt = {}
		row = row + 1
		splt = ParseCSVLine (line, ',') 
		table.insert(data, splt)
		rows = rows + 1
	   end
	end
	return rows
end

function mysort(t_in)
    -- battery data was added after RAPL data, so sort by timestamp
    -- RAPL data is collected by perf stat so RAPL variables collected in same 'batch' will have same timestamp 
    -- so sort by timestamp, then event then area within event
	--t[col.event] = evt
        -- printf("%.9f %s %.9f %.9f evt= %s\n", t[col.ts], t[col.area], t[col.watts], t[col.dura], evt)
    table.sort(t_in, function(a, b)
        if a[col.ts] ~= b[col.ts] then
            return a[col.ts] < b[col.ts]
        end
        if a[col.event] ~= b[col.event] then
            return a[col.event] < b[col.event]
        end
        return a[col.area] < b[col.area]
    end)
end

function myprt(t_in)
	--t[col.event] = evt
    printf("begin lua data table myprt after sort\n")
    local k, v
    for k,t in ipairs(t_in) do
        printf("%.9f %.9f %.9f evt= %s area= %s\n", t[col.ts], t[col.val], t[col.dura], t[col.event], t[col.area])
    end
    printf("end lua data table myprt\n")
end

function spin(flnm_spin, flnm_energy2, flnm_wait, verbose)
	print "in lua routine spin"
	local ts0, line, a, b, c

	filename_spin = flnm_spin
	printf("lua spin file= %s\n", filename_spin)
	ts0 = -1.0
	--      0.100126144	0.99	Joules	power/energy-pkg/	100092277	100.00
	--col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["watts"] = "watts", ["area"] = "area", ["event"]= "event"}
	col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["val"] = "val", ["area"] = "area", ["event"]= "event"}
	local rows = 0
	local hdrs = {}
	local tdata = {}
	local data = {}
	local k, v, k1, v1, evt
	rows = rows + read_file(filename_spin, hdrs, tdata, verbose)
	local row_max = rows
	local ts_idx = -1
	local col_max = -1
--  data[1][1]= cpu[0]: tid= 3190
--  data[1][2]=  beg/end= 356.615005
--  data[1][3]= 360.617361
--  data[1][4]=  dura= 4.002356
--  data[1][5]=  Gops= 0.000000
--  data[1][6]=  GB/sec= 14.273197
	local cpu, ts_beg, ts_end, ts_cur, dura, perf
	local evt_str = "mem_bw"
	evt_hash[evt_str] = 0
	ts_beg = -1
	rows = 0
	if verbose > 0 then
		printf("messages from spin.lua\n")
	end
--cpu[1]: tid= 6368, beg/end= 36273.157614,36277.157892, dura= 4.000278, Gops= 0.000000, GB/sec= 4.487594
	for k,t in ipairs(tdata) do
		for k1,v1 in ipairs(tdata[k]) do
			--printf("data[%s][%s]= %s\n", k, k1, v1);
			if k1 == 1 then
				local b1, e1 = string.find(v1, "cpu%[")
				local b2, e2 = string.find(v1, "]")
				cpu = string.sub(v1, b1+4, b2-1)
				if verbose > 0 then
					printf("cpu= %s\n", cpu)
				end
			end
			--if k1 == 4 then
				--local b, e = string.find(v1, "beg/end= ")
				--ts_cur = tonumber(string.sub(v1, b+9))
			if k1 == 3 then
				ts_cur = tonumber(v1)
				if verbose > 0 then
					printf("ts_cur= %s, v1= %s\n", ts_cur, v1)
				end
			end
			if k1 == 4 then
				local b, e = string.find(v1, "dura= ")
				dura = tonumber(string.sub(v1, b+6))
				if verbose > 0 then
					printf("dura= %s\n", dura)
				end
			end
			if k1 == 6 then
				local b, e = string.find(v1, "GB/sec= ")
				if b == nil then
					b, e = string.find(v1, "Gops/sec= ")
				end
				perf = tonumber(string.sub(v1, b+8))
				if verbose > 0 then
					printf("perf= %s\n", perf)
				end
				tm_end = ts_cur
				tm_beg = tm_end - dura
				if ts0 == -1 then
					ts0 = tm_beg
					ts_beg = ts0
				end
				ts_end = tm_end
				local t = {}
				t[col.ts] = tm_end
				t[col.val] = perf
				t[col.dura] = dura
				t[col.event] = evt_str
				t[col.area]  = cpu
				evt_hash[evt_str] = evt_hash[evt_str] + 1
				table.insert(data_table, t)
				rows = rows + 1
			end
		end
		col_max = k
	end

	--ts_delta = (ts_end - ts_beg)/rows
	--ts0 = ts_beg
	if verbose > 0 then
		printf("using ts0= %f\n", ts0)
	end

	local dura = ts_delta
	evt_units_hash[evt_str] = "val"

   mysort(data_table)
   --if verbose > 0 then
   --   myprt(data_table)
   --end
   local events = 0
   local event_nms = {}
   data_shape['col_names'] = {}
   data_shape['col_types'] = {}
   for k,v in pairs(evt_hash) do
	events = events + 1
	table.insert(event_nms, k)
	printf("lua event[%d]= %s\n", events, event_nms[events])
	data_shape['col_names'][events] = {col.ts, evt_units_hash[k], col.dura, col.area, col.event}
   	data_shape['col_types'][events] = {"dbl",  "dbl",             "dbl",    "str",    "str"}
   end
   --col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["val"] = "val", ["event"]= "event"}

   cols = 0
   for k,v in pairs(col) do
	   cols = cols + 1
   end
   data_shape['event_name'] = {}
   for k = 1, events, 1
   do
       printf("lua event[%d]= %s\n", k, event_nms[k])
       data_shape['event_name'][k] = event_nms[k]
   end
   data_shape['events'] = events
   printf("in lua: data_shape['rows'] = %d\n", rows)
   printf("in lua: data_shape['cols'] = %d\n", cols)
   data_shape['rows'] = rows;
   data_shape['cols'] = cols
   data_shape['event_area'] = 'lua'

   return
end

