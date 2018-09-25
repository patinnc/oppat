-- Copyright (c) 2018 Patrick Fay
--
-- License http://opensource.org/licenses/mit-license.php MIT License

function got_problems( error_msg )
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


function ext4_direct_IO(verbose)
	--print "in lua routine ext4_direct_IO"
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
		local tbl_ck_new_cols = {"MiB/s", "duration", "bytes", "area"}
		for k,t in ipairs(tbl_ck_new_cols) do
			if new_cols_hash[t] == nil then
				error("expected to find field '"..t.."' in new_cols list")
			end
		end
		local tbl_ck_data_cols = {"event", "ts", "extra_str", "comm", "pid", "tid"}
		for k,t in ipairs(tbl_ck_data_cols) do
			if data_cols_hash[t] == nil then
				error("expected to find field '"..t.."' in data_cols list")
			end
		end
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
	local cur_state = -1
	local evt = data_vals[data_cols_hash["event"]]
	local ts  = tonumber(data_vals[data_cols_hash["ts"]])
	local ky = data_vals[data_cols_hash["extra_str"]]
	local bytes = 0
	local b, e = string.find(ky, " rw ")
	if b == nil then
		error("got problem with ext4_direct_IO_enter/exit str= "..ky.." ts= "..ts)
	end
	local rdwr = string.sub(ky, e+1, e+1)
	--printf("in lua rdwr= %s\n", rdwr)
	local action
	if rdwr == "0" then
		action = "rd"
	else
		action = "wr"
	end
	ky = string.sub(ky, 1, b)
	b, e = string.find(ky, " len ")
	if b == nil then
		error("got problem with ext4_direct_IO_enter/exit str= "..ky.." ts= "..ts)
	end
	bytes = tonumber(string.sub(ky, e))
	--if evt == "ext4_direct_IO_enter" then 
	--	printf("ky= '%s', bytes= %d\n", ky, bytes);
	--end
--	spin.x-17593 [001] 15110116275907: ext4_direct_IO_enter: dev 8,6 ino 302391 pos 104792064 len 65536 rw 1
--	spin.x-17593 [001] 15110116443521: ext4_direct_IO_exit:  dev 8,6 ino 302391 pos 104792064 len 65536 rw 1 ret 65536

	if tbl[ky] == nil then
		tbl[ky] = {ts, bytes}
	else
		local new_val = ts - tbl[ky][1]
		local MiBs_idx = new_cols_hash["MiB/s"];
		local dura_idx  = new_cols_hash["duration"];
		local bytes_idx = new_cols_hash["bytes"];
		local area_idx  = new_cols_hash["area"];
		local comm_idx  = data_cols_hash["comm"];
		local pid_idx   = data_cols_hash["pid"];
		local tid_idx   = data_cols_hash["tid"];
		local str = data_vals[comm_idx] .. " " ..  data_vals[pid_idx] .. " " .. data_vals[tid_idx] .. " " .. action
		-- below assumes ts is in ns
		local ns_factor = 1.0e-9
		new_vals[dura_idx]   = ns_factor * new_val
		new_vals[bytes_idx]  = tbl[ky][2]

		--new_vals[area_idx]   = "system"
		new_vals[area_idx]   = str
		if new_val > 0.0 then
			-- below assumes ts is in ns
			new_vals[MiBs_idx] = tbl[ky][2] / (ns_factor * new_val)
		else
			new_vals[MiBs_idx] = 0.0;
		end
		--printf("ext4_direct_IO in lua= %d, MB/s= %f, str= %s, rdwr= %s\n", new_val, new_vals[MiBs_idx], str, rdwr)
		tbl[ky] = nil
	end
end
