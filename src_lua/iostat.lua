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

function read_file3(flnm, data, verbose)
	local rows = 0
	local row = -1
	local line
	local file = io.open(flnm, "r");
	for line in file:lines() do
		table.insert (data, line);
		if verbose > 0 then
			printf("line[%d], len= %d, %s\n", rows, string.len(line), line)
		end
		row = row + 1
		rows = rows + 1
	end
	io.close(file)
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
        printf("%.9f %.9f %.9f evt= %s\n", t[col.ts], t[col.line], t[col.dura], t[col.event])
    end
    printf("end lua data table myprt\n")
end

function trim1(s)
   return (s:gsub("^%s*(.-)%s*$", "%1"))
end

--t_first= 1328.822694500
--t_raw= 1328.841283191
--t_coarse= 1328.817997785
--t_boot= 1328.822770706
--t_mono= 1328.822775238
--gettimeofday= 1556598684.297533035

function iostat(file1, file2, file3, verbose)
   print "in lua routine do_tst"
   local ts0, line, a, b, c

   printf("lua file1= %s, file2= %s, file3= %s\n", file1, file2, file3)
   ts0 = -1.0
   ts0 = 0
   col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["kBpsec"] = "kBpsec", ["extra_str"] = "extra_str", ["area"] = "area", ["event"]= "event"}
   local clk_tbl = {}
   local clk_sz = read_file3(file1, clk_tbl, verbose)
	local k
	local ts_mono, ts_epoch
	for k = 1, clk_sz, 1 do
		if string.sub(clk_tbl[k], 1,7) == "t_mono=" then
			local tm_str = string.sub(clk_tbl[k], 9)
			ts_mono = tonumber(tm_str)
		end
		if string.sub(clk_tbl[k], 1,13) == "gettimeofday=" then
			local tm_str = string.sub(clk_tbl[k], 15)
			ts_epoch = tonumber(tm_str)
		end
	end
	printf("ts_mono= %f, ts_epoch= %f\n", ts_mono, ts_epoch)
   local file_tbl = {}
   local rows = 0
   local nsz = read_file3(file2, file_tbl, verbose)
   local v, evt
   local evt_str = "iostat"
   evt_hash[evt_str] = 0
   printf("got line[%d]= '%s'\n", nsz, file_tbl[nsz])
-- sammple data
--0        1
--1234567890123456789
--2019-04-30T00:31:23-0400
--Device             tps    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn
--mmcblk0         597.00      6200.00         0.00       6200          0

	if file_tbl[nsz] == nil then
		nsz = nsz -1
	end
	local w
	local dtbl = {}
	local timestamps = 0
	local dall_sz = 0
	local dall_tbl = {}
	local epoch_prv = -1
	local t_prv = -1
	k=3
	while (k <= nsz) do
		local dtbl2 = {}
		local s = file_tbl[k]
		if s == "" or s == nil then
			break
		end
		--printf("s[%d]= %s\n", k, s)
		local dt = {year=string.sub(s,1,4), month=string.sub(s,6,7), day=string.sub(s,9,10), hour=string.sub(s,12,13), min=string.sub(s,15,16), sec=string.sub(s,18,19)}
		local epoch_secs = os.time(dt) - ts_epoch + ts_mono
		--printf("s= %s dt= '%s' ts= %f\n", s, dump(dt), epoch_secs)
		timestamps = timestamps + 1
		dtbl2 = {["ts_str"]=s, ["ts"]=epoch_secs, ["dvals"]={}}
		if epoch_prv ~= epoch_secs then
			if epoch_prv == -1.0 then
				epoch_prv = epoch_secs - 1.0
			end
			t_prv = epoch_prv
			epoch_prv = epoch_secs
		end
		k = k + 2
		while (k <= nsz) do
			if file_tbl[k] == nil or file_tbl[k] == "" then
				k = k + 1
				break
			end
			--printf("k= %d, str= '%s'\n", k, file_tbl[k])
			local dura = epoch_secs - t_prv
			wtbl = {epoch_secs, s, dura}
	   		for w in file_tbl[k]:gmatch("%S+") do table.insert(wtbl, w) end
	   		table.insert(dtbl2["dvals"], wtbl)
			table.insert(dall_tbl, wtbl)
			dall_sz = dall_sz + 1
			k = k + 1
		end
		table.insert(dtbl, dtbl2)
	end
	printf("tbl= '%s'", dump(dall_tbl))


   printf("got line[%d]= '%s', tstr= '%s'\n", nsz, file_tbl[nsz], tstr)
   local dff = 0.5
   --for k,v in pairs(file_tbl) do
   local ts_prev = dall_tbl[1][1]
   local ts_t    = dall_tbl[2][1]
   local ts_init = dall_tbl[2][1]
   rows = 0
   for k = 1, dall_sz, 1
   do
	local v2 = dall_tbl[k]
--ts  tm_str  dura  Device             tps    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn
	local t = {}
	if v2[1] ~= ts_t then
		ts_prev = ts_t
	end
	t[col.ts] = v2[1]
	t[col.kBpsec] = v2[6]
	t[col.dura] = v2[3]
	t[col.extra_str] = v2[2]
	--printf("%f, %f, phas= %s\n", v2[1], v2[2], v2[3])
	t[col.event] = evt_str
	t[col.area]  = "rd_"..v2[4]
	evt_hash[evt_str] = evt_hash[evt_str] + 1
	if v2[1] ~= ts_init then
		table.insert(data_table, t)
		rows = rows + 1
	end
	local t2 = {}
	t2[col.ts] = v2[1]
	t2[col.kBpsec] = v2[7]
	t2[col.dura] = v2[3]
	t2[col.extra_str] = v2[2]
	--printf("%f, %f, phas= %s\n", v2[1], v2[2], v2[3])
	t2[col.event] = evt_str
	t2[col.area]  = "wr_"..v2[4]
	evt_hash[evt_str] = evt_hash[evt_str] + 1
	if v2[1] ~= ts_init then
		table.insert(data_table, t2)
		rows = rows + 1
	end
	ts_t = v2[1]
   end
   evt_units_hash[evt_str] = "kBpsec"
   -- this 'W' string is used as the lookup for event value in charts.json

   --mysort(data_table)
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

