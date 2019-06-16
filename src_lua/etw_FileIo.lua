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

-- FileIoRead,  TimeStamp,     Process Name ( PID),   ThreadID, LoggingProcessName ( PID), LoggingThreadID, CPU,             IrpPtr,         FileObject,   ByteOffset,       Size,      Flags, ExtraFlags, Priority, FileName, ParsedFlags
-- FileIoWrite,  TimeStamp,     Process Name ( PID),   ThreadID, LoggingProcessName ( PID), LoggingThreadID, CPU,             IrpPtr,         FileObject,   ByteOffset,       Size,      Flags, ExtraFlags, Priority, FileName, ParsedFlags
-- FileIoOpEnd,  TimeStamp,     Process Name ( PID),   ThreadID, LoggingProcessName ( PID), LoggingThreadID, CPU,             IrpPtr,         FileObject, ElapsedTime,     Status,          ExtraInfo, Type, FileName
--  SysCallEnter,    8304583,         spin.exe (11060),      11224, 0xfffff800552ae8d0,     ntoskrnl.exe!NtWriteFile
--   FileIoWrite,    8304585,         spin.exe (11060),      11224,        spin.exe (11060),             11224,   0, 0xffffe288851f8608, 0xffffe2889d1aa7d0, 0x00000000ce, 0x00000041, 0x00000000, 0x00000000,   NotSet, "C:\data\ppat\oppat_data\win\spin10_pcm\phase.tsv", 
--   FileIoOpEnd,    8304586,         spin.exe (11060),          0,        spin.exe (11060),             11224,   0, 0xffffe288851f8608, 0xffffe2889d1aa7d0,           2, 0xc01c0004, 0x0000000000000000, FileIoWrite, "C:\data\ppat\oppat_data\win\spin10_pcm\phase.tsv"
--   FileIoWrite,    8304588,         spin.exe (11060),      11224,        spin.exe (11060),             11224,   0, 0xffffe288851f8608, 0xffffe2889d1aa7d0, 0x00000000ce, 0x00000041, 0x00060a00, 0x00000000,   Normal, "C:\data\ppat\oppat_data\win\spin10_pcm\phase.tsv", write_operation | defer_io_completion | 17 | 18
--   FileIoRead,  TimeStamp,     Process Name ( PID),   ThreadID, LoggingProcessName ( PID), LoggingThreadID, CPU,             IrpPtr,         FileObject,   ByteOffset,       Size,      Flags, ExtraFlags, Priority, FileName, ParsedFlags
--   FileIoWrite,  TimeStamp,     Process Name ( PID),   ThreadID, LoggingProcessName ( PID), LoggingThreadID, CPU,             IrpPtr,         FileObject,   ByteOffset,       Size,      Flags, ExtraFlags, Priority, FileName, ParsedFlags
--   FileIoOpEnd,  TimeStamp,     Process Name ( PID),   ThreadID, LoggingProcessName ( PID), LoggingThreadID, CPU,             IrpPtr,         FileObject, ElapsedTime,     Status,          ExtraInfo, Type, FileName
--   FileIoOpEnd,    8304596,         spin.exe (11060),          0,        spin.exe (11060),             11224,   0, 0xffffe288851f8608, 0xffffe2889d1aa7d0,           8, 0x00000000, 0x0000000000000041, FileIoWrite, "C:\data\ppat\oppat_data\win\spin10_pcm\phase.tsv"
--  SysCallExit,    8304597,         spin.exe (11060),      11224, 0x00000000


function FileIo(verbose)
	--printf("in lua routine FileIo with der_evt_idx= %s\n", de)
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
		local tst = {"area", "__EMIT__", "bytes", "dura", "num", "den"}
		for k,t in ipairs(tst) do
			local idx = new_cols_hash[t]
			if idx == nil then
				error("expected to find field '"..t.."' in new_cols list")
			end
		end
		tst = {"Type", "ElapsedTime", "ExtraInfo"}
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
		type_idx  = data_cols_hash["Type"]
		extr_idx  = data_cols_hash["ExtraInfo"]
		time_idx  = data_cols_hash["ElapsedTime"]
		printf("area_idx = %s\n", area_idx)
	end
	if type(tbl) ~= "table" then
		tbl = {}
	end
	local cur_state = -1
	local myevt, mystate
	new_vals[emit_idx] = 0
	--log_pid = data_cols_hash["LoggingProcessName ( PID)"]
	--log_tid = data_cols_hash["LoggingThreadID"]
	--log_irp = data_cols_hash["IrpPtr"]
	--log_file= data_cols_hash["FileObject"]
	if data_vals[1] == "FileIoOpEnd" then
		local log_type = data_vals[type_idx]
		--printf("got FileIo type= %s\n", log_type)
		if log_type == "FileIoRead" or log_type == "FileIoWrite" then
			local log_ext  = tonumber(data_vals[extr_idx])
			local dura = tonumber(data_vals[time_idx])
			if verbose > 0 then
				printf("got FileIo evt= %s, dura= %d bytes= %d\n", log_type, dura, log_ext)
			end
			if log_ext > 0 then
				mystate  = 2
				myevt    = "end"
				new_vals[num_idx] = log_ext
				new_vals[den_idx] = dura
				new_vals[area_idx] = log_type
				new_vals[byte_idx] = log_ext
				new_vals[dura_idx] = dura
				new_vals[emit_idx] = 1
			end
		end
	end
end
