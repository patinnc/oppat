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
	-- procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu----- -----timestamp-----
	-- r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st                 EDT
	local t = {}
	t[col.ts] = t2[1]
	t[col.dura] = t2[3]
	t[col.extra_str] = "vmstat time= "..t2[2]
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

function vmstat(file1, file2, file3, verbose)
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
	for w2 in hdr1:gmatch("%S+") do table.insert(hdr1_tbl, w2) end
	for w2 in hdr2:gmatch("%S+") do table.insert(hdr2_tbl, w2) end
	-- ck hdr1
	-- procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu----- -----timestamp-----
	-- r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st                 EDT
	-- 2  0      0 354500  50972 383684    0    0    70     4 2946  191  0  1 98  1  0 2019-04-30 00:31:22
	-- 1  1      0 342216  53620 392268    0    0  6168     0 14929 5651  3 11 76 11  0 2019-04-30 00:31:23

	local hdr1_ck = {"procs", "memory", "swap", "io", "system", "cpu", "timestamp"}
	for k = 1, 7, 1
	do
		if hdr1_tbl[k]:find(hdr1_ck[k]) == nil then
			printf("didn't find string[%d]= '%s' in vmstat header1='%s'\n", k, hdr1_ck[k], file_tbl[1])
			error("check vmstat command line options or vmstat.txt data file")
		end
	end

	-- area for each hdr column. 
	local hdr2_area = {"proc", "proc", "mem",  "mem",  "mem",  "mem",   "swap", "swap", "io", "io", "sys", "sys", "cpu", "cpu", "cpu", "cpu", "cpu"}
	local hdr2_ck =   {"r",    "b",    "swpd", "free", "buff", "cache", "si",   "so",   "bi", "bo", "in",  "cs",  "us",  "sy",  "id",  "wa",  "st"}
	local area_lkup = {"runnable", "blocked", "virt_mem_used", "free_mem", "mem_used_as_buffer", "mem_used_as_cache", "mem_swapped_in_from_disk/s",   "mem_swapped_out_to_disk/s",   "blocks_from_block_dev/s", "blocks_to_block_dev/s", "interrupts/s",  "cswitch/s",  "%user",  "%sytem",  "%idle",  "%waiting_on_IO",  "%stolen_from_vm"}
	local f = {}
	for k = 1, 17, 1
	do
		f[hdr2_ck[k]] = k
		if hdr2_tbl[k]:find(hdr2_ck[k]) == nil then
			printf("didn't find string[%d]= '%s' in vmstat header2='%s'\n", k, hdr2_ck[k], file_tbl[2])
			error("check vmstat command line options or vmstat.txt data file")
		end
	end

	k=3
	t_prv = -1
	local dall_sz = 0
	local dall_tbl = {}
	local tall_tbl = {}
	while (k <= nsz) do
		local dtbl = {}
	   	for w in file_tbl[k]:gmatch("%S+") do table.insert(dtbl, w) end
		local s = dtbl[18] .. " " .. dtbl[19]
		local dt = {year=string.sub(s,1,4), month=string.sub(s,6,7), day=string.sub(s,9,10), hour=string.sub(s,12,13), min=string.sub(s,15,16), sec=string.sub(s,18,19)}
		--printf("s[%d]= '%s'\n", dall_sz, s)
		local epoch_secs = os.time(dt) - ts_epoch + ts_mono
		--printf("s= %s dt= '%s' ts= %f\n", s, dump(dt), epoch_secs)
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
	--rows = gen_evt(tall_tbl[k], dall_tbl[k][f.free], col, evt_str, 'system', evt_hash, data_table, rows, evt_units_hash)
	--             timestamps   value for this var,  dall_cols,  name of event for chart, chart area,  hsh of events, out data, num of rows, hsh of string used for this event's data (the by_var='string' in charts.json)

	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.buff],  col, "vmstat."..hdr2_area[f.buff], area_lkup[f.buff],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.free],  col, "vmstat."..hdr2_area[f.buff], area_lkup[f.free],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.cache], col, "vmstat."..hdr2_area[f.buff], area_lkup[f.cache], evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.swpd],  col, "vmstat."..hdr2_area[f.buff], area_lkup[f.swpd],  evt_hash, data_table, rows, evt_units_hash)

	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.r],  col, "vmstat."..hdr2_area[f.r], area_lkup[f.r],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.b],  col, "vmstat."..hdr2_area[f.r], area_lkup[f.b],  evt_hash, data_table, rows, evt_units_hash)

	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.si],  col, "vmstat."..hdr2_area[f.si], area_lkup[f.si],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.so],  col, "vmstat."..hdr2_area[f.si], area_lkup[f.so],  evt_hash, data_table, rows, evt_units_hash)

	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.bi],  col, "vmstat."..hdr2_area[f.bi], area_lkup[f.bi],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.bo],  col, "vmstat."..hdr2_area[f.bo], area_lkup[f.bo],  evt_hash, data_table, rows, evt_units_hash)

	-- f.in generates an error
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.cs-1],  col, "vmstat."..hdr2_area[f.cs], area_lkup[f.cs-1],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.cs],  col, "vmstat."..hdr2_area[f.cs], area_lkup[f.cs],  evt_hash, data_table, rows, evt_units_hash)

	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.us],  col, "vmstat."..hdr2_area[f.us], area_lkup[f.us],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.sy],  col, "vmstat."..hdr2_area[f.sy], area_lkup[f.sy],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.id],  col, "vmstat."..hdr2_area[f.id], area_lkup[f.id],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.wa],  col, "vmstat."..hdr2_area[f.wa], area_lkup[f.wa],  evt_hash, data_table, rows, evt_units_hash)
	rows = gen_evt(tall_tbl[k], dall_tbl[k][f.st],  col, "vmstat."..hdr2_area[f.st], area_lkup[f.st],  evt_hash, data_table, rows, evt_units_hash)
	-- procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu----- -----timestamp-----
	-- r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st                 EDT
   end
   --evt_units_hash[evt_str] = "val"
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

