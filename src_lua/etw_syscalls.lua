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

--   SysCallEnter,  TimeStamp,     Process Name ( PID),   ThreadID,        ServiceAddr,            Image!Function
--   SysCallExit,  TimeStamp,     Process Name ( PID),   ThreadID,     Status
--   SysCallEnter,      87836,        xperf.exe (4872),       3764, 0xfffff800552cf900,     ntoskrnl.exe!NtAllocateVirtualMemory
--   SysCallExit,      87840,        xperf.exe (4872),       3764, 0x00000000
--   SysCallEnter,      87851,        xperf.exe (4872),       3764, 0xfffff800552cf900,     ntoskrnl.exe!NtAllocateVirtualMemory
--   SysCallExit,      87853,        xperf.exe (4872),       3764, 0x00000000

function syscalls_all(verbose)
	--printf("in lua routine syscalls\n")
	local ts0, ts
	local vrb = verbose
	--vrb = 1

	if type(new_cols_hash) ~= "table" or new_cols_hash == nil then
		new_cols_hash = {}
		data_cols_hash = {}
		for k,t in ipairs(new_cols) do
			printf("evt etw_sockets new_cols: k= %s t= %s\n", k, t)
    		new_cols_hash[t] = k
		end
		for k,t in ipairs(data_cols) do
			printf("evt etw_sockets data_cols: k= %s t= %s\n", k, t)
    		data_cols_hash[t] = k
		end
		--local tst = {"area", "duration", "__EMIT__"}
		local tst = {"area", "duration"}
		for k,t in ipairs(tst) do
			local idx = new_cols_hash[t]
			if idx == nil then
				error("expected to find field '"..t.."' in new_cols list")
			end
		end
--		tst = {"ElapsedTime", "IOSize"}
--		for k,t in ipairs(tst) do
--			if data_cols_hash[t] == nil then
--				error("expected to find field '"..t.."' in data_cols list")
--			end
--		end
		area_idx  = new_cols_hash["area"]
		dura_idx  = new_cols_hash["duration"]
		--emit_idx  = new_cols_hash["__EMIT__"]
	end

	if type(tbl) ~= "table" then
		tbl = {}
	end
	local cur_state = -1
	local myevt, mystate
	if data_vals[1] == "SysCallEnter" then
		mystate = 0
	else
		mystate = 1
	end
	if mystate == 0 then
		local arr = mysplit(data_vals[6], "!")
		myevt = arr[2]
	end
	local ky = data_vals[4] .. " " .. data_vals[3]
	--if vrb > 0 then
	--	printf("lua syscall %s: ky=%s, cur_state= %s\n", data_vals[1], ky, myevt)
	--end
	--new_vals[emit_idx] = 0
	if mystate == 0 then
		if tbl[ky] == nil then
			tbl[ky] = {data_vals[2], myevt}
		end
	else
		if tbl[ky] ~= nil then
			local ts = tbl[ky][1]
			myevt = tbl[ky][2]
			new_val = tonumber(data_vals[2]) - tonumber(ts)
			if vrb > 0 then
				printf("new_val in lua= %d\n", new_val)
			end
			new_vals[dura_idx] = new_val
			new_vals[area_idx] = myevt
			if verbose > 0 then
				printf("syscalls area= %s, dura= %d\n", myevt, new_val)
			end
			tbl[ky] = nil
			--new_vals[emit_idx] = 1
		else
			new_vals[1] = 0.0
		end
	end
end
