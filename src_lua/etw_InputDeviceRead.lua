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

function InputDeviceRead(verbose)
	if verbose > 0 then
		print "in lua routine InputDeviceRead"
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
	local splt = {}
	splt = mysplit(data_cols[1], "/:")
	--for k,t in ipairs(splt) do
    --    printf("splt[%s]= %s\n", k, splt[k])
	--end
	local myevt   = splt[2]
	local mystate = splt[4]
	--print splt
	if  mystate == "Start" then
		cur_state = 0
	elseif mystate == "Stop" then
		cur_state = 1
	end
	if cur_state == -1 then
		local info = debug.getinfo(1,'S')
		printf("what is going on here, got unexpected evt type= %s in lua file %s\n", data_cols[1], info.source)
		return
	end
	local ky = myevt .. " " .. data_vals[3] .. " " .. data_vals[4]
	if verbose > 0 then
		printf("lua InputDeviceRead: ky=%s, cur_state= %d\n", ky, cur_state)
	end
	if cur_state == 0 then
		tbl[ky] = tonumber(data_vals[2])
		if verbose > 0 then
			printf("tbl[%s]= %d\n", ky, tbl[ky])
		end
	else
		if tbl[ky] == nil then
			new_val = 0
		else
			new_val = tonumber(data_vals[2]) - tbl[ky]
			if verbose > 0 then
				printf("new_val in lua= %d\n", new_val)
			end
			if new_cols[1] ~= "duration" then
				error("expected first new_col to be 'duration', got: "..new_cols[1])
			end
			new_vals[1] = new_val
		end
		tbl[ky] = nil
	end
   --return
end

