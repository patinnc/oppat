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

function read_file(ts0, flnm)
   local rows = 0
   local ts_prev = -1.0
   local ts_last = {}
   local tm = {}
   local tm_prev = {}
   local units = ""
   local evt
   local imc_sum = 0
   local imc_state = 0
   local try_lines = 0
   local extr_j = 0
   local skipped_rows =0
   for line in io.lines(flnm) do
      if (string.len(line) > 30 and string.sub(line, 1, 1) ~= "#") then
	local t = {}
	local str
        local j = 0
   	try_lines = try_lines + 1
	j = -1
        extr_j = 0
        for i in string.gmatch(line, "%S+") do
           j = j + 1
	   if (j == 0) then
	      --printf("i= %s, ts0= %s, line= %s\n", i, ts0, line)
              t[col.ts] = tonumber(i)+ts0  -- time offset + ts0
              ts_last[1] = i 
              ts_last[2] = t[col.ts]
	   elseif (j == 1) then
              if i == "<not" then
                 skipped_rows = skipped_rows + 1
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
		if i == "unc_arb_trk_requests.all" then
			extr_j = 1
			units = "bytes"
		end
	   	local b2, e2 = string.find(i, "-faults")
	    if b2 ~= nil then
			extr_j = 1
			units = "faults"
		end
 	   end
	   if ((j+extr_j) == 3) then
	   	local b2, e2 = string.find(i, "-faults")
	    if b2 ~= nil then
			t[col.area] = i
			evt = "faults"
		elseif i == "thermal/temperature/" then
			t[col.area] = 'system'
			evt = "temperature"
		elseif i == "power/energy-pkg/" then
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
		elseif i == "unc_arb_trk_requests.all" then
                 	t[col.area] = 'uncore'
			evt = "unc_bytes"
		elseif i == "uncore_imc/data_reads/" then
               		t[col.area] = 'read'
			evt = "imc_bw"
			imc_state = imc_state | 1
			imc_sum = imc_sum + t[col.watts]
		elseif i == "uncore_imc/data_writes/" then
                 	t[col.area] = 'write'
			evt = "imc_bw"
			imc_state = imc_state | 2
			imc_sum = imc_sum + t[col.watts]
		else
			printf("unhandled lua evt %s in %s\n", i, flnm);
			--break
		end
		if evt_hash[evt] == nil then
			evt_hash[evt] = 0
		end
		evt_hash[evt] = evt_hash[evt] + 1
		evt_units_hash[evt] = units
		--printf("evt_hsh[%s] = 1, ck %s\n", evt, evt_hash[evt])
		t[col.event] = evt
		tm[t[col.area]] = t[col.ts]
	   elseif ((j+extr_j) == 4) then
	      local v = 1.0e-9 * tonumber(i)
              if tm_prev[t[col.area]] == nil then
                 tm_prev[t[col.area]] = t[col.ts] - v
              end
              local elap = t[col.ts] - tm_prev[t[col.area]]
   	      if units == 'Joules' or units == 'MiB' or units == 'cycles' then
                  t[col.watts] = t[col.watts] / elap
	      end
	      t[evt_units_hash[evt]] = t[col.watts]
              t[col.dura] = elap
              -- printf("%.9f %s %.9f %.9f evt= %s\n", t[col.ts], t[col.area], t[col.watts], t[col.dura], evt)
              table.insert(data_table, t)
              if imc_state == 3 then
              		--tm_prev[t[col.area]] = t[col.ts]
			local t2 = {}
              		t2[col.dura] = elap
                  	t2[col.watts] = imc_sum / elap
                 	evt = 'imc_bw'
	      		t2[evt_units_hash[evt]] = t2[col.watts]
			t2[col.event] = evt
                 	t2[col.area] = 'sum'
			t2[col.ts] = t[col.ts]
			if evt_hash[evt] == nil then
				evt_hash[evt] = 0
			end
			evt_hash[evt] = evt_hash[evt] + 1
			evt_units_hash[evt] = units
              		table.insert(data_table, t2)
                        rows = rows + 1
			imc_state = 0
			imc_sum   = 0
              end
              ts_last[3] = t[col.ts]
              tm_prev[t[col.area]] = t[col.ts]
              rows = rows + 1
           end
        end
      end
   end
   if rows > 0 then
   printf("lua_file %s ts_last ts[1]= %f, ts[2]= %s, ts[3]= %s try_lines= %d, rows= %d, skipped= %d, trws= %d\n",
      flnm, ts_last[1], ts_last[2], ts_last[3], try_lines, rows, skipped_rows, skipped_rows+rows);
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
    local rws = 0
    for k,t in ipairs(t_in) do
        printf("%.9f %s %.9f %.9f evt= %s\n", t[col.ts], t[col.area], t[col.watts], t[col.dura], t[col.event])
	rws = rws + 1
    end
    printf("end lua data table myprt, rws= %d\n", rws)
end

function do_tst(flnm_energy, flnm_energy2, flnm_wait, verbose)
   print "in lua routine do_tst"
   local ts0, line, a, b, c

   filename_wait_txt = flnm_wait
   --filename_energy_txt = "prf_energy.txt"
   filename_energy_txt = flnm_energy
   filename_energy2_txt = flnm_energy2
   printf("lua energy file= %s, energy2 file= %s, wait file= %s\n", filename_energy_txt, filename_energy2_txt, flnm_wait)
   ts0 = -1.0
--t_first= 15522.682034838
   for line in io.lines(filename_wait_txt) do
      if (string.sub(line, 1, 8) == "t_first=") then
        ts0 = string.sub(line, 10)
        printf("got lua t_first ts0= %f from file %s\n", ts0, filename_wait_txt)
        break
      end
   end
   if ts0 == -1.0 then
      printf("didn't find 't_start= xxx' string in %s. bye\n", filename_wait_txt)
      return
   end
   --      0.100126144	0.99	Joules	power/energy-pkg/	100092277	100.00
   col = {["ts"] = "_TIMESTAMP_", ["dura"] = "_DURATION_", ["watts"] = "watts", ["area"] = "area", ["event"]= "event"}
   local rows = 0
   local events = 0
   local event_nms = {}
   local k, v, evt
   local rows_energy = 0
   if filename_energy_txt ~= "" then
   	rows_energy = read_file(ts0, filename_energy_txt)
   	rows = rows + rows_energy
   end
   local rows_energy2 = 0
   if filename_energy2_txt ~= "" then
   	rows_energy2 = read_file(ts0, filename_energy2_txt)
   	rows = rows + rows_energy2
   end
   mysort(data_table)
   if verbose > 0 then
      myprt(data_table)
   end
   data_shape['col_names'] = {}
   data_shape['col_types'] = {}
   for k,v in pairs(evt_hash) do
	events = events + 1
	table.insert(event_nms, k)
	printf("col_types: lua event[%d]= %s\n", events, event_nms[events])
	data_shape['col_names'][events] = {col.ts, col.area, evt_units_hash[k], col.dura, col.event}
   	data_shape['col_types'][events] = {"dbl",  "str",    "dbl",             "dbl",    "str"}
   end

   cols = 5
   data_shape['event_name'] = {}
   for k = 1, events, 1
   do
       printf("data_shape: lua event[%d]= %s\n", k, event_nms[k])
       data_shape['event_name'][k] = event_nms[k]
   end
   data_shape['events'] = events
   printf("rows= %d, rows_energy= %d rows_energy2= %d\n", rows, rows_energy, rows_energy2)
   data_shape['rows'] = rows;
   data_shape['cols'] = cols
   data_shape['event_area'] = 'lua'

   --return
end

