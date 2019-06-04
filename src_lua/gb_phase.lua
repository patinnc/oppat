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

function read_file2(flnm, data)
   local rows = 0
   local row = -1
   for line in io.lines(flnm) do
	printf("line[%d]= %s\n", rows, line)
	local splt = {}
	row = row + 1
	splt = mysplit(line, '\t')
	table.insert(data, splt)
	rows = rows + 1
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
		local ts0, ts1
        ts0 = tonumber(a[col.ts])
        ts1 = tonumber(b[col.ts])
        if ts0 ~= ts1 then
            return ts0 < ts1
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
        printf("%.9f %.9f %.9f evt= %s\n", t[col.ts], t[col.line], t[col.dura], t[col.event])
    end
    printf("end lua data table myprt\n")
end

function gb_phase(file1, file2, file3, verbose)
   print "in lua routine do_tst"
   local ts0, line, a, b, c

   printf("lua file1= %s, file2= %s, file3= %s\n", file1, file2, file3)
   ts0 = -1.0
--t_first= 15522.682034838
   ts0 = 0
   --      0.100126144	0.99	Joules	power/energy-pkg/	100092277	100.00
   --col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["watts"] = "watts", ["area"] = "area", ["event"]= "event"}
   col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["marker"] = "marker", ["extra_str"] = "extra_str", ["area"] = "area", ["event"]= "event"}
   local file_tbl = {}
   local rows = 0
   local nsz = read_file2(file2, file_tbl)
   local j, k, v, evt
   -- 'gen_line1' must match event_name field in charts.json
   local evt_str = "gb_phase"
   evt_hash[evt_str] = 0
   local dff = 0.5
   --for k,v in pairs(file_tbl) do
   local dff_sml = 0.00001
   for k = 1, nsz, 1
   do
     local v2 = file_tbl[k]
	 local gbv4
	 local sngl_mlti = string.sub(v2[3], 1, 6)
	 --printf("sngl_mlti= %s\n", sngl_mlti)
	 if sngl_mlti == "Single" or sngl_mlti == "Multi-" then
		 gbv4 = true
	 else
		gbv4 = false
   		dff_sml = 0.0;
	 end
	 for j = 1, 2, 1 do
		if j == 1 or gbv4 == true then
		 local t = {}
		 local adj = dff_sml
		 if j == 2 then
			 adj = 0.0
		 end
		 -- so... the rows are sort laster by timestamp and we want the non-zero dura to appear before the zero dura row
		 t[col.ts] = tostring(tonumber(v2[1]) - adj)
		 rows = rows + 1
		 t[col.marker] = rows
		 t[col.dura] = tostring(tonumber(v2[2]) - adj)
		 t[col.extra_str] = v2[3]
		 if j == 2 then
			 -- these are place holder events and the dura will be filled later in oppat.cpp
		 	t[col.dura] = "0.0"
		 	t[col.extra_str] = "idle: " .. v2[3]
		 end
		 printf("%f, %f, phas= %s\n", v2[1], t[col.dura], t[col.extra_str])
		 t[col.event] = evt_str
		 t[col.area]  = "test"
		 evt_hash[evt_str] = evt_hash[evt_str] + 1
		 table.insert(data_table, t)
	 	end
	 end
   end
   evt_units_hash[evt_str] = "marker"
   -- this 'W' string is used as the lookup for event value in charts.json

   mysort(data_table)
   --if verbose > 0 then
      --myprt(data_table)
   local events = 0
   local event_nms = {}
   data_shape['col_names'] = {}
   data_shape['col_types'] = {}
   for k,v in pairs(evt_hash) do
	events = events + 1
	table.insert(event_nms, k)
	printf("lua event[%d]= %s\n", events, event_nms[events])
	data_shape['col_names'][events] = {col.ts, evt_units_hash[k], col.dura, col.area, col.event, col.extra_str, col.marker}
   	data_shape['col_types'][events] = {"dbl",  "dbl",             "dbl",    "str",    "str",     "str",     "dbl"}
   end
   --col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["line"] = "line", ["event"]= "event"}

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

   --return
end

