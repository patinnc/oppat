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

function etw_tm_since_prev_evt(verbose)
	--print "in lua routine tm_since_last_evt"
	local ts0

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
	local evt = data_vals[1]
	local ts  = tonumber(data_vals[2])
	local ky = evt
	if tbl[ky] == nil then
		new_val = 0
	else
		new_val = ts - tbl[ky]
		printf("etw_tm_since_prev_evt in lua= %d\n", new_val)
		if new_cols[1] ~= "duration" then
			error("expected first new_col to be 'duration', got: "..new_cols[1])
		end
		new_vals[1] = new_val
	end
	tbl[ky] = ts
end
