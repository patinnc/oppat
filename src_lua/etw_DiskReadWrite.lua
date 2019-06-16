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

-- DiskRead,  TimeStamp,     Process Name ( PID),   ThreadID, CPU,             IrpPtr,   ByteOffset,     IOSize, ElapsedTime,  DiskNum, IrpFlags, DiskSvcTime, I/O Pri,  VolSnap,         FileObject, FileName
-- DiskWrite,  TimeStamp,     Process Name ( PID),   ThreadID, CPU,             IrpPtr,   ByteOffset,     IOSize, ElapsedTime,  DiskNum, IrpFlags, DiskSvcTime, I/O Pri,  VolSnap,         FileObject, FileName
-- DiskRead,       8612,        xperf.exe (3984),      11844,   1, 0xffffe288b23f1b20, 0x018f0b8000, 0x00001000,         283,        0, 0x060043,         283,       3, Original, 0xffffe288272d0d20, "C:\$Mft"

function DiskReadWrite(verbose)
	--printf("in lua routine DiskReadWrite\n")
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
			printf("evt etw_DiskReadWrite new_cols: k= %s t= %s\n", k, t)
    		new_cols_hash[t] = k
		end
		for k,t in ipairs(data_cols) do
			printf("evt etw_DiskReadWrite data_cols: k= %s t= %s\n", k, t)
    		data_cols_hash[t] = k
		end
		local tst = {"area", "__EMIT__", "num", "den"}
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
		area_idx = new_cols_hash["area"]
		emit_idx = new_cols_hash["__EMIT__"]
		num_idx  = new_cols_hash["num"]
		den_idx  = new_cols_hash["den"]
		time_idx = data_cols_hash["ElapsedTime"]
		size_idx = data_cols_hash["IOSize"]
		printf("area_idx = %s\n", area_idx)
	end
	if type(tbl) ~= "table" then
		tbl = {}
	end
	local cur_state = -1
	local myevt, mystate
	if data_vals[1] == "DiskRead" then
		mystate = 0
		myevt = "read"
	else
		mystate = 1
		myevt = "write"
	end
	new_vals[area_idx] = myevt
	new_vals[emit_idx] = 1
	local bytes = tonumber(data_vals[size_idx])
	local dura  = tonumber(data_vals[time_idx])
	new_vals[num_idx] = bytes
	new_vals[den_idx] = dura
	local mbps = 0.0
	if dura > 0.0 then
		mbps = bytes/dura
	end
	if verbose > 0 then
		printf("DiskReadWrite: area= %s, bytes= %d, tm= %d, MB/s= %.3f\n", myevt, bytes, dura, mbps)
	end
end
