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

-- TcpSend,  TimeStamp,     Process Name ( PID), Transfer Size, Remote IPv4 Addr, Remote IPv4 Port, Local IPv4 Addr, Local IPv4 Port
-- TcpRecv,  TimeStamp,     Process Name ( PID), Transfer Size, Remote IPv4 Addr, Remote IPv4 Port, Local IPv4 Addr, Local IPv4 Port



function etw_tcp(verbose)
	--printf("in lua routine etw_tcp sockets\n")
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
			printf("evt etw_sockets new_cols: k= %s t= %s\n", k, t)
    		new_cols_hash[t] = k
		end
		for k,t in ipairs(data_cols) do
			printf("evt etw_sockets data_cols: k= %s t= %s\n", k, t)
    		data_cols_hash[t] = k
		end
		local tst = {"area", "__EMIT__", "bytes", "dura", "num", "den"}
		for k,t in ipairs(tst) do
			local idx = new_cols_hash[t]
			if idx == nil then
				error("expected to find field '"..t.."' in new_cols list")
			end
		end
		tst = {"Transfer Size"}
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
		size_idx  = data_cols_hash["Transfer Size"]
		printf("area_idx = %s\n", area_idx)
	end
	if type(tbl) ~= "table" then
		tbl = {}
	end
	--new_vals[emit_idx] = 0
	local sz = tonumber(data_vals[size_idx])
	--local dura = tonumber(data_vals[time_idx])
	local dura = 1.0e-6
	new_vals[area_idx] = data_vals[1]
	new_vals[byte_idx] = sz
	new_vals[num_idx]  = sz
	new_vals[den_idx]  = 1.0
	new_vals[dura_idx] = dura
	new_vals[emit_idx] = 1
	if verbose > 0 then
		printf("got tcp area= %s, bytes= %.3f dura= %.3f\n", new_vals[area_idx], sz, dura)
	end
end
