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

--              ps-11330 [003] 167433.182969843: sys_enter_getegid:    
--              ps-11330 [003] 167433.182972239: sys_exit_getegid:     0x0
--              ps-11330 [003] 167433.182974947: sys_enter_getuid:     
--              ps-11330 [003] 167433.182977291: sys_exit_getuid:      0x0
--              ps-11330 [003] 167433.182980208: sys_enter_getgid:     
--              ps-11330 [003] 167433.182982968: sys_exit_getgid:      0x0
--              ps-11330 [003] 167433.182985520: sys_enter_faccessat:  dfd: 0xffffffffffffff9c, filename: 0xaaaaf0974b10, mode: 0x00000004
--              ps-11330 [003] 167433.182998541: sys_exit_faccessat:   0x0
--              ps-11330 [003] 167433.183002812: sys_enter_newfstatat: dfd: 0xffffffffffffff9c, filename: 0xaaaaf0974b10, statbuf: 0xfffff0bc5ea8, flag: 0x00000000
--              ps-11330 [003] 167433.183013281: sys_exit_newfstatat:  0x0
--              ps-11330 [003] 167433.183016666: sys_enter_newfstatat: dfd: 0xffffffffffffff9c, filename: 0xaaaaf0974b10, statbuf: 0xfffff0bc5de8, flag: 0x00000000
--            grep-11331 [001] 167433.183021093: sys_enter_close:      fd: 0x00000004
--            grep-11331 [001] 167433.183026302: sys_exit_close:       0x0
--              ps-11330 [003] 167433.183026718: sys_exit_newfstatat:  0x0


function syscalls(verbose)
	--print "in lua routine syscalls"
	local ts0

	if type(new_cols_hash2) ~= "table" then
		new_cols_hash2 = {}
		for k,t in ipairs(new_cols) do
    		new_cols_hash2[t] = k
		end
		data_cols_hash2 = {}
		for k,t in ipairs(data_cols) do
    		data_cols_hash2[t] = k
		end
		local tst = {"MiB/s", "duration", "bytes", "area"}
		for k,t in ipairs(tst) do
			local idx = new_cols_hash2[t]
			if idx == nil then
				error("expected to find field '"..t.."' in new_cols list")
			end
		end
		tst = {"event", "ts", "extra_str", "comm", "pid", "tid"}
		for k,t in ipairs(tst) do
			if data_cols_hash2[t] == nil then
				error("expected to find field '"..t.."' in data_cols list")
			end
		end
		MiBs_idx2      = new_cols_hash2["MiB/s"]
		dura_idx2      = new_cols_hash2["duration"]
		bytes_idx2     = new_cols_hash2["bytes"]
		area_idx2      = new_cols_hash2["area"]
		emit_idx2      = new_cols_hash2["__EMIT__"];
		event_idx2     = data_cols_hash2["event"]
		ts_idx2        = data_cols_hash2["ts"]
		extra_str_idx2 = data_cols_hash2["extra_str"]
		comm_idx2      = data_cols_hash2["comm"]
		pid_idx2       = data_cols_hash2["pid"]
		lkup_tbl2 = {}
		level2 = {}
	end
	--for k,t in ipairs(data_cols) do
    --    printf("data_cols[%s]= %s\n", k, data_cols[k])
	--end
	--for k,t in ipairs(data_vals) do
    --    printf("data_vals[%s]= %s\n", k, data_vals[k])
	--end
	if type(tbl) ~= "table" then
		tbl = {}
	end
	local evt = data_vals[event_idx2]
	local ts  = tonumber(data_vals[ts_idx2])
	local ky  = data_vals[extra_str_idx2]
	local comm  = data_vals[comm_idx2]
	local pid   = data_vals[pid_idx2]
	new_vals[emit_idx2] = 0
	-- ps-11330 [003] 167433.185336300: sys_enter_read:       fd: 0x00000003, buf: 0xffffdbe15600, count: 0x00000340
	--printf("syscalls: evt= %s\n",evt)

	local ev = nil
	local stage = 0
	if string.sub(evt, 1, 10) == "sys_enter_" then
		ev = string.sub(evt, 11)
		stage = 1
	elseif string.sub(evt, 1, 9) == "sys_exit_" then
		ev = string.sub(evt, 10)
		stage = 2
	end
	-- this logic assumes there won't be than 1 syscall per process+thread outstanding.
	-- That is, for example, a thread will only have 1 sys_enter_read outstanding and
	-- that the next sys_exit_read event applies to the 1 outstanding sys_enter_read.
	-- The sys_exit_* events only have a return code arg.
	-- I guess it would be easy enough to check if we have more than 1 outstanding
	-- "process + thread + event_type" at a time.
	local prv = nil
	local lkup = nil
	if ev == "read" or ev == "write" or ev == "pread64" or ev == "pwrite64" then
		--printf("ts= %f, evt= %s, ev= %s, comm= %s, pid= %s\n", ts, evt, ev, comm, pid);
		lkup = pid .." "..comm.." "..ev
		if stage == 1 then
			local arr = mysplit(ky, " ")
			if string.sub(arr[6], -1) == "," then
				arr[6] = string.sub(arr[6], 1, string.len(arr[6])-1)
			end
			lkup_tbl2[lkup] = {ts, tonumber(arr[6])}
			--printf("ts= %f, evt= %s, comm= %s, pid= %s, count= %s\n", ts, evt, comm, pid, arr[6]);
		elseif stage == 2 then
			prv = lkup_tbl2[lkup]
		end
	end
	if prv ~= nil then
		local ts_diff = ts - prv[1]
		local ns_factor = 1.0e-9
		-- below assumes ts is in ns
		new_vals[dura_idx2] = ns_factor * ts_diff
		new_vals[bytes_idx2]  = prv[2]
		--new_vals[area_idx2] = ev..":"..comm..":"..pid
		new_vals[area_idx2] = ev
		new_vals[emit_idx2] = 1
		--printf("bytes= %s, dura= %s\n", prv[2], new_vals[dura_idx2])
		if ts_diff > 0.0 then
			new_vals[MiBs_idx2] = prv[2] / new_vals[dura_idx2]
		else
			new_vals[MiBs_idx2] = 0.0;
		end
		lkup_tbl2[lkup] = nil
	end
end

function syscalls_all(verbose)
	--print "in lua routine syscalls"
	local ts0

	if type(new_cols_hash) ~= "table" then
		new_cols_hash = {}
		for k,t in ipairs(new_cols) do
    		new_cols_hash[t] = k
		end
		data_cols_hash = {}
		for k,t in ipairs(data_cols) do
    		data_cols_hash[t] = k
		end
		local tst = {"MiB/s", "duration", "bytes", "area"}
		for k,t in ipairs(tst) do
			local idx = new_cols_hash[t]
			if idx == nil then
				error("expected to find field '"..t.."' in new_cols list")
			end
		end
		tst = {"event", "ts", "extra_str", "comm", "pid", "tid"}
		for k,t in ipairs(tst) do
			if data_cols_hash[t] == nil then
				error("expected to find field '"..t.."' in data_cols list")
			end
		end
		MiBs_idx      = new_cols_hash["MiB/s"]
		dura_idx      = new_cols_hash["duration"]
		dura2_idx     = new_cols_hash["dura2"]
		bytes_idx     = new_cols_hash["bytes"]
		area_idx      = new_cols_hash["area"]
		emit_idx      = new_cols_hash["__EMIT__"];
		event_idx     = data_cols_hash["event"]
		ts_idx        = data_cols_hash["ts"]
		extra_str_idx = data_cols_hash["extra_str"]
		comm_idx      = data_cols_hash["comm"]
		pid_idx       = data_cols_hash["pid"]
		pid_idx       = data_cols_hash["tid"]
		lkup_tbl = {}
		--level = {}
	end
	--for k,t in ipairs(data_cols) do
    --    printf("data_cols[%s]= %s\n", k, data_cols[k])
	--end
	--for k,t in ipairs(data_vals) do
    --    printf("data_vals[%s]= %s\n", k, data_vals[k])
	--end
	if type(tbl) ~= "table" then
		tbl = {}
	end
	local evt = data_vals[event_idx]
	local ts  = tonumber(data_vals[ts_idx])
	local ky  = data_vals[extra_str_idx]
	local comm  = data_vals[comm_idx]
	local pid   = data_vals[pid_idx]
	new_vals[emit_idx] = 0
	-- ps-11330 [003] 167433.185336300: sys_enter_read:       fd: 0x00000003, buf: 0xffffdbe15600, count: 0x00000340
	--printf("syscalls: evt= %s\n",evt)

	local ev = nil
	local stage = 0
	if string.sub(evt, 1, 10) == "sys_enter_" then
		ev = string.sub(evt, 11)
		stage = 1
	elseif string.sub(evt, 1, 9) == "sys_exit_" then
		ev = string.sub(evt, 10)
		stage = 2
	end
	--if level[ev] == nil then
	--	level[ev] = 0
	--end
	-- this logic assumes there won't be than 1 syscall per process+thread outstanding.
	-- That is, for example, a thread will only have 1 sys_enter_read outstanding and
	-- that the next sys_exit_read event applies to the 1 outstanding sys_enter_read.
	-- The sys_exit_* events only have a return code arg.
	-- I guess it would be easy enough to check if we have more than 1 outstanding
	-- "process + thread + event_type" at a time.
	-- The 'already defined' and 'not defined' msgs are an indicator of how valid the
	-- assumption is. 
	-- For the arm_multi9 data, we get ~3 'already defined' (perf's execve syscalls)
	-- and about 20 'not defined' (various wait, poll, select, nanosleep, etc).
	-- The 'not defined' msgs are not too surprising. The 'already defined' msgs
	-- are worrisome.
	local prv = nil
	local lkup = nil
	--if ev == "read" or ev == "write" or ev == "pread64" or ev == "pwrite64" then
		--printf("ts= %f, evt= %s, ev= %s, comm= %s, pid= %s\n", ts, evt, ev, comm, pid);
		lkup = pid .." "..comm.." "..ev
		if stage == 1 then
			--local arr = mysplit(ky, " ")
			--if string.sub(arr[6], -1) == "," then
			--	arr[6] = string.sub(arr[6], 1, string.len(arr[6])-1)
			--end
			if lkup_tbl[lkup] ~= nil then
				local tid   = data_vals[pid_idx]
				printf("syscall already defined: ts= %f, evt= %s, comm= %s, pid= %s tid= %s ext= '%s', lkup= %s\n",
					1.0e-9*ts, evt, comm, pid, tid, ky, lkup);
			end
			--level[ev] = level[ev] + 1
			lkup_tbl[lkup] = {ts}
			--printf("ts= %f, evt= %s, comm= %s, pid= %s, count= %s\n", ts, evt, comm, pid, arr[6]);
		elseif stage == 2 then
			prv = lkup_tbl[lkup]
			if lkup_tbl[lkup] == nil then
				printf("syscall not defined: ts= %f, evt= %s, comm= %s, pid= %s ext= '%s', lkup= %s\n", 1.0e-9*ts, evt, comm, pid, ky, lkup);
			end
		end
	--end
	if prv ~= nil then
		local ts_diff = ts - prv[1]
		local ns_factor = 1.0e-9
		-- below assumes ts is in ns
		new_vals[dura_idx] = ns_factor * ts_diff
		--new_vals[dura2_idx] = level[ev]
		new_vals[dura2_idx] = 1.0
		new_vals[bytes_idx]  = 0.0
		new_vals[area_idx] = ev
		--new_vals[area_idx] = ev..":"..comm..":"..pid
		new_vals[emit_idx] = 1
		--printf("bytes= %s, dura= %s\n", prv[2], new_vals[dura_idx])
		new_vals[MiBs_idx] = 0.0;
		--if ts_diff > 0.0 then
		--	new_vals[MiBs_idx] = prv[2] / new_vals[dura_idx]
		--else
		--	new_vals[MiBs_idx] = 0.0;
		--end
		--level[ev] = level[ev] - 1
		--if level[ev] < 0 then
		--	level[ev] = 0
		--end
		lkup_tbl[lkup] = nil
	end
end
