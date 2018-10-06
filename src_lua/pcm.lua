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

function read_file(flnm, hdr, data)
   local rows = 0
   local row = -1
   for line in io.lines(flnm) do
		local splt = {}
		row = row + 1
		splt = ParseCSVLine (line, ';') 
		if row == 0 then
			table.insert(hdr, splt);
		elseif row == 1 then
			table.insert(hdr, splt);
		else
			table.insert(data, splt)
			rows = rows + 1
		end
	end
	return rows
end

--[[
function old_stf()
	if 1 == 20 then
		printf("splt= '%s', len= %d, spl1= '%s' 2= '%s'\n", line, #splt, splt[1], splt[23])
		error("bye at lua")
      if (string.len(line) > 30 and string.sub(line, 1, 1) ~= "#") then
		local t = {}
		local str
        local j = 0
		local extr_j = 0
        for i in string.gmatch(line, "%S+") do
			if (j == 0) then
	      --printf("i= %s, ts0= %s, line= %s\n", i, ts0, line)
              t[col.ts] = tonumber(i)+ts0  -- time offset + ts0
			elseif (j == 1) then
              if i == "<not" then
                 break -- event <not supported> by perf
			end
			t[col.watts] = tonumber(i) -- joules
	   end
	   if (j == 2) then
   		units = i
		if i == "uncore_cbox_0/clockticks/" or i == "uncore_cbox_1/clockticks/" then
			extr_j = 1
			units = "cycles"
		end
 	   end
	   if ((j+extr_j) == 3) then
		if i == "power/energy-pkg/" then
			t[col.area] = 'pkg'
			evt = "power"
		elseif i == "power/energy-cores/" then
			t[col.area] = 'cpu'
			evt = "power"
		elseif i == "power/energy-gpu/" then
			t[col.area] = 'gpu'
			evt = "power"
		elseif i == "power/energy-ram/" then
			t[col.area] = 'ram'
			evt = "power"
		elseif i == "power/battery/" then
			t[col.area] = 'bat'
			evt = "power"
		elseif i == "uncore_cbox_0/clockticks/" then
               		t[col.area] = 'cbox0'
			evt = "cbox_freq"
		elseif i == "uncore_cbox_1/clockticks/" then
                 	t[col.area] = 'cbox1'
			evt = "cbox_freq"
		elseif i == "uncore_imc/data_reads/" then
               		t[col.area] = 'imc_reads'
			evt = "imc_bw"
			imc_state = imc_state | 1
			imc_sum = imc_sum + t[col.watts]
		elseif i == "uncore_imc/data_writes/" then
                 	t[col.area] = 'imc_writes'
			evt = "imc_bw"
			imc_state = imc_state | 2
			imc_sum = imc_sum + t[col.watts]
		else
			break
		end
		if evt_hash[evt] == nil then
			evt_hash[evt] = 0
		end
		evt_hash[evt] = evt_hash[evt] + 1
		evt_units_hash[evt] = units
		--printf("evt_hsh[%s] = 1, ck %s\n", evt, evt_hash[evt])
		t[col.event] = evt
		tm[t[col.area] ] = t[col.ts]
	   elseif ((j+extr_j) == 4) then
	      local v = 1.0e-9 * tonumber(i)
              if tm_prev[t[col.area] ] == nil then
                 tm_prev[t[col.area] ] = t[col.ts] - v
              end
              local elap = t[col.ts] - tm_prev[t[col.area] ]
   	      if units == 'Joules' or units == 'MiB' or units == 'cycles' then
                  t[col.watts] = t[col.watts] / elap
	      end
	      t[evt_units_hash[evt] ] = t[col.watts]
              t[col.dura] = elap
              -- printf("%.9f %s %.9f %.9f evt= %s\n", t[col.ts], t[col.area], t[col.watts], t[col.dura], evt)
              table.insert(data_table, t)
		if imc_state == 3 then
              		--tm_prev[t[col.area] ] = t[col.ts]
			local t2 = {}
              		t2[col.dura] = elap
                  	t2[col.watts] = imc_sum / elap
                 	evt = 'imc_bw'
	      		t2[evt_units_hash[evt] ] = t2[col.watts]
			t2[col.event] = evt
                 	t2[col.area] = 'imc_sum'
			t2[col.ts] = t[col.ts]
			if evt_hash[evt] == nil then
				evt_hash[evt] = 0
			end
			evt_hash[evt] = evt_hash[evt] + 1
			evt_units_hash[evt] = units
              		table.insert(data_table, t2)
			imc_state = 0
			imc_sum   = 0
		end
              tm_prev[t[col.area] ] = t[col.ts]
              rows = rows + 1
           end
           j = j + 1
        end
      end
   end
   return rows
end
]]

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

function read_pcm(flnm_pcm, flnm_energy2, flnm_wait, verbose)
	print "in lua routine read_pcm"
	local ts0, line, a, b, c

	filename_pcm = flnm_pcm
	printf("lua pcm file= %s\n", filename_pcm)
	ts0 = -1.0
	--      0.100126144	0.99	Joules	power/energy-pkg/	100092277	100.00
	--col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["watts"] = "watts", ["area"] = "area", ["event"]= "event"}
	col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["val"] = "val", ["area"] = "area", ["event"]= "event"}
	local rows = 0
	local hdrs = {}
	local data = {}
	local k, v, evt
	rows = rows + read_file(filename_pcm, hdrs, data)
	local row_max = rows
	printf("%s\n", hdrs[1][1])
	local ts_idx = -1
	local col_max = -1
	for k,t in ipairs(hdrs[2]) do
		if t == "TimeStamp" then
			ts_idx = k
			--break
		end
		col_max = k
	end
	if ts_idx == -1 then
		error("expected 2nd line of PCM file '" .. flnm_pcm .. "' to have a column header 'TimeStamp'. Bye")
	end

	local ts_beg, ts_end, ts_delta
	ts_beg = data[1][ts_idx]
	ts_end = data[rows][ts_idx]
	ts_delta = (ts_end - ts_beg)/rows
	ts0 = ts_beg
	printf("using ave ts_delta = %f, ts0= %f\n", ts_delta, ts0)

	local evt_str = "pcm_event"
	evt_hash[evt_str] = 0
	local r, c, cur_hdr0
	local dura = ts_delta
	rows = 0
	for r = 1, row_max, 1 do
		cur_hdr0 = hdrs[1][1]
		printf("r= %d, dura= %f, ts= %f\n", r, dura, data[r][ts_idx])
		for c = ts_idx+1, col_max, 1 do
			local t = {}
			if hdrs[1][c] ~= "" then
				cur_hdr0 = hdrs[1][c]
			end
			t[col.ts] = data[r][ts_idx]
			t[col.val] = data[r][c]
			if r > 1 then
				dura = data[r][ts_idx] - data[r-1][ts_idx]
			end
			t[col.dura] = dura
			t[col.event] = evt_str
			t[col.area]  = cur_hdr0 .. hdrs[2][c]
			local b, e = string.find(t[col.area], "Joules")
			if b ~= nil and dura > 0.0 then
				t[col.val] = t[col.val] / dura
			end
			if dura > 0.0 and (hdrs[2][c] == "READ" or hdrs[2][c] == "WRITE") then
				t[col.val] = t[col.val] / dura
			end

			evt_hash[evt_str] = evt_hash[evt_str] + 1
			table.insert(data_table, t)
			rows = rows + 1
		end
	end
	evt_units_hash[evt_str] = "val"
   -- this 'W' string is used as the lookup for event value in charts.json

   mysort(data_table)
   if verbose > 0 then
      myprt(data_table)
   end
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

