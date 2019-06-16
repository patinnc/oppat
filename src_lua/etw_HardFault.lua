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

-- HardFault,  TimeStamp,     Process Name ( PID),   ThreadID,        VirtualAddr,   ByteOffset,     IOSize, ElapsedTime,         FileObject, FileName, Hardfaulted Address Information
-- HardFault,       8729,        xperf.exe (3984),      11844, 0xffffa88fbf2b8000, 0x00053b8000, 0x00001000,         425, 0xffffe288272d0d20, "$Mft", N/A



function HardFault(verbose)
	--printf("in lua routine HardFault with der_evt_idx= %s\n", de)
	local ts0, ts
	local vrb = verbose
	--vrb = 1


	--if type(new_cols_hash) ~= "table" then
	--	new_cols_hash = {}
	--	data_cols_hash = {}
	--end
	if type(new_cols_hash) ~= "table" or new_cols_hash == nil then
		new_cols_hash = {}
		data_cols_hash = {}
		for k,t in ipairs(new_cols) do
			printf("evt etw_HardFault new_cols: k= %s t= %s\n", k, t)
    		new_cols_hash[t] = k
		end
		for k,t in ipairs(data_cols) do
			printf("evt etw_HardFault data_cols: k= %s t= %s\n", k, t)
    		data_cols_hash[t] = k
		end
		local tst = {"area", "__EMIT__", "bytes", "dura", "num", "den"}
		for k,t in ipairs(tst) do
			local idx = new_cols_hash[t]
			if idx == nil then
				error("expected to find field '"..t.."' in new_cols list")
			end
		end
		tst = {"ElapsedTime", "IOSize"}
		for k,t in ipairs(tst) do
			if data_cols_hash[t] == nil then
				error("expected to find field '"..t.."' in data_cols list")
			end
		end
		byte_idx  = new_cols_hash["bytes"]
		area_idx  = new_cols_hash["area"]
		emit_idx  = new_cols_hash["__EMIT__"]
		dura_idx  = new_cols_hash["dura"]
		num_idx  = new_cols_hash["num"]
		den_idx  = new_cols_hash["den"]
		size_idx  = data_cols_hash["IOSize"]
		time_idx  = data_cols_hash["ElapsedTime"]
		printf("area_idx = %s\n", area_idx)
	end
	if type(tbl) ~= "table" then
		tbl = {}
	end
	new_vals[emit_idx] = 0
	--log_pid = data_cols_hash["LoggingProcessName ( PID)"]
	--log_tid = data_cols_hash["LoggingThreadID"]
	--log_irp = data_cols_hash["IrpPtr"]
	--log_file= data_cols_hash["FileObject"]
	if data_vals[1] == "HardFault" then
		--printf("got HardFault type= %s\n", log_type)
		local log_size = tonumber(data_vals[size_idx])
		local dura = tonumber(data_vals[time_idx])
		new_vals[area_idx] = data_vals[1]
		new_vals[byte_idx] = log_size
		new_vals[num_idx]  = log_size
		new_vals[den_idx]  = dura
		new_vals[dura_idx] = dura
		new_vals[emit_idx] = 1
		if verbose > 0 then
			printf("got HardFault area= %s, bytes= %d dura= %d\n", new_vals[area_idx], log_size, dura)
		end
	end
end
