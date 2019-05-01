
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

function read_file3(flnm, data)
   local rows = 0
   local row = -1
   for line in io.lines(flnm) do
	printf("line[%d]= %s\n", rows, line)
	--local splt = {}
	row = row + 1
	--splt = mysplit(line, '\t')
	table.insert(data, line)
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

function gen_evt(t2, val, col, evt_str, area, evt_hash, data_table, rows, evt_units_hash)
	local t = {}
	t[col.ts] = t2[1]
	t[col.dura] = t2[3]
	t[col.extra_str] = "nicstat time= "..t2[2]
	t[col.val] = val
	t[col.event] = evt_str
	t[col.area]  = area
	if evt_hash[evt_str] == nil then
		evt_hash[evt_str] = 0
   		evt_units_hash[evt_str] = "val"
	end
	evt_hash[evt_str] = evt_hash[evt_str] + 1
	table.insert(data_table, t)
	rows = rows + 1
	return rows
end

--t_first= 1328.822694500
--t_raw= 1328.841283191
--t_coarse= 1328.817997785
--t_boot= 1328.822770706
--t_mono= 1328.822775238
--gettimeofday= 1556598684.297533035

function nicstat(file1, file2, file3, verbose)
   print "in lua routine do_tst"
   local ts0, line, a, b, c
   local json = require("src_lua.json")

   printf("lua file1= %s, file2= %s, file3= %s\n", file1, file2, file3)
   ts0 = -1.0
--t_first= 15522.682034838
   ts0 = 0
   --      0.100126144	0.99	Joules	power/energy-pkg/	100092277	100.00
   --col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["watts"] = "watts", ["area"] = "area", ["event"]= "event"}
   col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["val"] = "val", ["extra_str"] = "extra_str", ["area"] = "area", ["event"]= "event"}
   local clk_tbl = {}
   local clk_sz = read_file3(file1, clk_tbl)
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
   local nsz = read_file3(file2, file_tbl)
   local v, evt
   -- 'gen_line1' must match event_name field in charts.json
   printf("got line[%d]= '%s'\n", nsz, file_tbl[nsz])
--0        1
--1234567890123456789
--2019-04-30T00:31:23-0400
--Device             tps    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn
--mmcblk0         597.00      6200.00         0.00       6200          0

	if file_tbl[nsz] == nil then
		nsz = nsz -1
	end
	local w
	local timestamps = 0
	local t_prv = -1
	local w2
	local hdr1 = file_tbl[1]
	local hdr2 = file_tbl[2]
	local hdr1_tbl = {}
	local hdr2_tbl = {}
	--for w2 in hdr1:gmatch("%S+") do table.insert(hdr1_tbl, w2) end
	--for w2 in hdr2:gmatch("%S+") do table.insert(hdr2_tbl, w2) end
	-- ck hdr1
-- Time      Int   rKB/s   wKB/s   rPk/s   wPk/s    rAvs    wAvs %Util
--1556598682:wlan0:0.940:0.177:7.889:0.792:0.00:0.00
--1556598682:lo:0.007:0.007:0.090:0.090:0.00:0.00

	--local hdr1_ck = {"Time", "Interface", "Kbytes/sec", "Kbytes/sec", "packets/sec", "packets/sec"}
	-- cmd 'nicstat -p 1' doesn't put a header in the data file
	--for k = 1, 6, 1
	--do
	--	if hdr1_tbl[k]:find(hdr1_ck[k]) == nil then
	--		printf("didn't find string[%d]= '%s' in nicstat header1='%s'\n", k, hdr1_ck[k], file_tbl[1])
	--		error("check nicstat command line options or nicstat.txt data file")
	--	end
	--end

	-- area for each hdr column. 
	local hdr2_area = {"Time", "Interface", "KBytes/sec", "KBytes/sec", "packets/sec", "packets/sec"}
	local hdr2_ck =   {"Time", "Interface", "readKB/s",   "writeKB/s",  "readPkt/sec", "writePkt/sec"}
	local hdr2_f  =   {"time", "int",       "rKBps",      "wKBps",      "rPps",        "wPps"}
	local f = {}
	for k = 1, 6, 1
	do
		f[hdr2_f[k]] = k
		-- 'nicstat -p 1' doesn't put headers in so don't check
		--if hdr2_tbl[k]:find(hdr2_ck[k]) == nil then
		--	printf("didn't find string[%d]= '%s' in nicstat header2='%s'\n", k, hdr2_ck[k], file_tbl[2])
		--	error("check nicstat command line options or nicstat.txt data file")
		--end
	end

	k=1
	t_prv = -1
	local dall_sz = 0
	local dall_tbl = {}
	local tall_tbl = {}
	while (k <= nsz) do
		local dtbl = {}
	   	--for w in file_tbl[k]:gmatch(":") do table.insert(dtbl, w) end
	   	dtbl = mysplit(file_tbl[k], ":")
		local s = dtbl[1] -- epoch secs
		printf("s= %s\n", s)
		local epoch_secs = tonumber(s) - ts_epoch + ts_mono
		timestamps = timestamps + 1
		local dura = epoch_secs - t_prv
		if dall_sz == 1 then
			-- dura of 1st row isn't really known... just set it to diff between 2nd and 1st timestamp.
			tall_tbl[1][3] = dura
		end
		local ttbl = {epoch_secs, s, dura}
		table.insert(dall_tbl, dtbl)
		table.insert(tall_tbl, ttbl)
		dall_sz = dall_sz + 1
		t_prv = epoch_secs
		k = k + 1
	end
	printf("tbl= '%s'", dump(dall_tbl))


   printf("got line[%d]= '%s', tstr= '%s'\n", nsz, file_tbl[nsz], tstr)
   rows = 0
   for k = 1, dall_sz, 1
   do
	--             timestamps   value for this var,  dall_cols,  name of event for chart, chart area,  hsh of events, out data, num of rows, hsh of string used for this event's data (the by_var='string' in charts.json)
	--local hdr2_f  =   {"time", "int",       "rKBps",      "wKBps",      "rPps",        "wPps"}

	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.rKBps],  col, "nicstat."..hdr2_area[f.rKBps], dall_tbl[k][f.int].."_"..hdr2_ck[f.rKBps],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.wKBps],  col, "nicstat."..hdr2_area[f.wKBps], dall_tbl[k][f.int].."_"..hdr2_ck[f.wKBps],  evt_hash, data_table, rows, evt_units_hash)

	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.rPps],  col, "nicstat."..hdr2_area[f.rPps], dall_tbl[k][f.int].."_"..hdr2_ck[f.rPps],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.wPps],  col, "nicstat."..hdr2_area[f.wPps], dall_tbl[k][f.int].."_"..hdr2_ck[f.wPps],  evt_hash, data_table, rows, evt_units_hash)
   end

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

