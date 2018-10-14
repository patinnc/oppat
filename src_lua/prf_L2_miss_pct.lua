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


function L2_miss_pct(verbose)
	--print "in lua routine L2_miss_pct"
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
		local tbl_ck_new_cols = {"L2_miss_pct", "__EMIT__", "duration", "area"}
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
			tbl_prev = {}
			tbl_ts   = {}
		end
	end
	local evt = data_vals[data_cols_hash["event"]]
	local ts  = data_vals[data_cols_hash["ts"]]
	local cpu = tonumber(data_vals[data_cols_hash["cpu"]])
	local ts_num = tonumber(ts)
	local per = tonumber(data_vals[data_cols_hash["period"]])
	tbl_ts[cpu] = ts_num
	if tbl[cpu] == nil then
		tbl[cpu] = {0, 0, 0, 0, 0, 0}
		tbl_prev[cpu] = nil
	end
	if evt == 'r17' then
		tbl[cpu][2] = tbl[cpu][2] + per
		tbl[cpu][6] = tbl[cpu][4] -- save prev ts
		tbl[cpu][4] = ts_num
	end
	if evt == 'r16' then
		tbl[cpu][3] = tbl[cpu][3] + per
		tbl[cpu][5] = ts_num
	end
	local emit_idx = new_cols_hash["__EMIT__"];
	new_vals[emit_idx] = 0
	if tbl[cpu][4] > 0 and tbl[cpu][4] == tbl[cpu][5] then
		local L2_miss_pct_idx  = new_cols_hash["L2_miss_pct"];
		local area_idx = new_cols_hash["area"];
		local dura_idx = new_cols_hash["duration"];
		local numer  = tbl[cpu][2]
		local denom = tbl[cpu][3]
		local ts_cur = tbl[cpu][4]
		local ts_prv = tbl[cpu][6]
		local dura = 0.0
		if ts_prv > 0 then
			dura = 1.0e-9 * (ts_cur - ts_prv)
		end
		if dura > 0.0 and denom > 0.0 then
			local val = 100.0 * numer / denom
			--if ld_stl > 120.0 then
			--	ld_stl = 120.0
			--end
			new_vals[L2_miss_pct_idx] = val
			new_vals[area_idx] = cpu
			new_vals[emit_idx] = 1
			new_vals[dura_idx] = dura
			--printf("prf_L2_miss_pct.lua: ld_stl= %f, dura= %f, cpu= %d\n", val, dura, cpu)
		else
			new_vals[L2_miss_pct_idx] = 0.0
		end
		tbl[cpu][2] = 0
		tbl[cpu][3] = 0
		tbl_prev[cpu] = ts_cur
	end
end