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


function gen_div_pair(verbose)
	--print "in lua routine gen_dir_pair"
	if type(new_cols_hash) ~= "table" then
		new_cols_hash = {}
		for k,t in ipairs(new_cols) do
    		new_cols_hash[t] = k
		end
		data_cols_hash = {}
		for k,t in ipairs(data_cols) do
    		data_cols_hash[t] = k
		end
		local tbl_ck_new_cols = {"val", "__EMIT__", "duration", "area"}
		for k,t in ipairs(tbl_ck_new_cols) do
			if new_cols_hash[t] == nil then
				error("expected to find field '"..t.."' in new_cols list")
			end
		end
		local tbl_ck_data_cols = {"event", "cpu", "ts", "period"}
		for k,t in ipairs(tbl_ck_data_cols) do
			if data_cols_hash[t] == nil then
				error("expected to find field '"..t.."' in data_cols list")
			end
		end
		if type(tbl) ~= "table" then
			tbl = {}
			tbl_devt_hsh = {}
		end
	end
	local devt = data_vals[data_cols_hash["derived_evt"]]
	if tbl_devt_hsh[devt] == nil then
		local idx = 1
		for k,t in pairs(tbl_devt_hsh) do
			idx = idx + 1
		end
		tbl_devt_hsh[devt] = idx
	end
	local devt_idx = tbl_devt_hsh[devt]
	local evt  = data_vals[data_cols_hash["event"]]
	local evt_tag = data_vals[data_cols_hash["evt_tag"]]
	local ts  = data_vals[data_cols_hash["ts"]]
	local cpu = tonumber(data_vals[data_cols_hash["cpu"]])
	local ts_num = tonumber(ts)
	local per = tonumber(data_vals[data_cols_hash["period"]])
	if tbl[devt_idx] == nil then
		tbl[devt_idx] = {}
	end
	if tbl[devt_idx][cpu] == nil then
		tbl[devt_idx][cpu] = {0, 0, 0, 0, 0, 0}
	end
	if evt_tag == 'num' then
		--tbl[devt_idx][cpu][2] = tbl[devt_idx][cpu][2] + per
		tbl[devt_idx][cpu][2] = per
		tbl[devt_idx][cpu][6] = tbl[devt_idx][cpu][4] -- save prev ts
		tbl[devt_idx][cpu][4] = ts_num
		--printf("gen_div_pair.lua: devt= %s num cpu= %d, evt= %s, per= %f, ts= %f\n", devt, cpu, evt, per, 1.0e-9 *ts_num)
	end
	if evt_tag == 'den' then
		--tbl[devt_idx][cpu][3] = tbl[devt_idx][cpu][3] + per
		tbl[devt_idx][cpu][3] = per
		tbl[devt_idx][cpu][5] = ts_num
		--printf("gen_div_pair.lua: devt= %s den cpu= %d, evt= %s, per= %f, ts= %f\n", devt, cpu, evt, per, 1.0e-9 *ts_num)
	end
	local emit_idx = new_cols_hash["__EMIT__"];
	new_vals[emit_idx] = 0
	if tbl[devt_idx][cpu][4] > 0 and tbl[devt_idx][cpu][4] == tbl[devt_idx][cpu][5] then
		local val_idx  = new_cols_hash["val"];
		local area_idx = new_cols_hash["area"];
		local dura_idx = new_cols_hash["duration"];
		local num    = tbl[devt_idx][cpu][2]
		local den    = tbl[devt_idx][cpu][3]
		local ts_cur = tbl[devt_idx][cpu][4]
		local ts_prv = tbl[devt_idx][cpu][6]
		local dura = 0.0
		if ts_prv > 0 then
			dura = 1.0e-9 * (ts_cur - ts_prv)
		end
		if dura > 0.0 and den > 0.0 then
			local val = num / den
			new_vals[val_idx]  = val
			new_vals[area_idx] = cpu
			new_vals[emit_idx] = 1
			new_vals[dura_idx] = dura
			--printf("gen_div_pair.lua: devt= %s dn2 val= %f, dura= %f, cpu= %d, evt= %s, tag= %s, per= %f, ts= %f\n", devt, val, dura, cpu, evt, evt_tag, per, 1.0e-9 *ts)
		else
			new_vals[val_idx]  = 0.0
		end
		tbl[devt_idx][cpu][2] = 0
		tbl[devt_idx][cpu][3] = 0
	end
end
