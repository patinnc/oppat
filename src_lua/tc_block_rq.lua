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

--    jbd2/sda6-8-234   [002] 377123728938: block_rq_insert:      8,0 WS 196608 () 298927464 + 384 [jbd2/sda6-8]
--     jbd2/sda6-8-234   [002] 377123741307: block_rq_issue:       8,0 WS 196608 () 298927464 + 384 [jbd2/sda6-8]
--          <idle>-0     [002] 377143764759: block_rq_complete:    8,0 WS () 298927464 + 384 [0]
--            perf-3029  [000] 377143876643: ext4_da_write_begin:  dev 8,6 ino 299099 pos 87637 len 605 flags 0
--            perf-3029  [000] 377143897345: ext4_da_write_end:    dev 8,6 ino 299099 pos 87637 len 605 copied 605
--     jbd2/sda6-8-234   [002] 377143979991: block_rq_insert:      8,0 FWFS 4096 () 298927848 + 8 [jbd2/sda6-8]
--     jbd2/sda6-8-234   [002] 377143983378: block_rq_issue:       8,0 FF 0 () 18446744073709551615 + 0 [jbd2/sda6-8]
--          <idle>-0     [002] 377153681320: block_rq_complete:    8,0 FF () 18446744073709551615 + 0 [0]
--          <idle>-0     [002] 377153685429: block_rq_issue:       8,0 WS 4096 () 298927848 + 8 [swapper/2]
--          <idle>-0     [002] 377155356679: block_rq_complete:    8,0 WS () 298927848 + 8 [0]
--          <idle>-0     [002] 377155369281: block_rq_issue:       8,0 FF 0 () 18446744073709551615 + 0 [swapper/2]
--            perf-3029  [000] 377164220488: ext4_da_write_begin:  dev 8,6 ino 299099 pos 88242 len 614 flags 0
--            perf-3029  [000] 377164225336: ext4_da_write_end:    dev 8,6 ino 299099 pos 88242 len 614 copied 614
--          <idle>-0     [002] 377165174266: block_rq_complete:    8,0 FF () 18446744073709551615 + 0 [0]
--          <idle>-0     [002] 377165178623: block_rq_complete:    8,0 WS () 298927848 + 0 [0]

function block_rq(verbose)
	--print "in lua routine block_rq"
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
		local tst = {"MiB/s", "duration", "bytes", "area"}
		for k,t in ipairs(tst) do
			local idx = new_cols_hash[t]
			if idx == nil then
				error("expected to find field '"..t.."' in new_cols list")
			end
		end
		tst = {"event", "ts", "extra_str", "comm", "pid", "tid"}
		for k,t in ipairs(tst) do
			if data_cols_hash[t] == nil then
				error("expected to find field '"..t.."' in data_cols list")
			end
		end
		MiBs_idx      = new_cols_hash["MiB/s"]
		dura_idx      = new_cols_hash["duration"]
		bytes_idx     = new_cols_hash["bytes"]
		area_idx      = new_cols_hash["area"]
		emit_idx      = new_cols_hash["__EMIT__"];
		event_idx     = data_cols_hash["event"]
		ts_idx        = data_cols_hash["ts"]
		extra_str_idx = data_cols_hash["extra_str"]
		lkup_tbl = {}
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
	local evt = data_vals[event_idx]
	local ts  = tonumber(data_vals[ts_idx])
	local ky  = data_vals[extra_str_idx]
	local bytes = 0
	local arr = mysplit(ky, " ")
	local lkup
	local use_ky
	new_vals[emit_idx] = 0
	if evt == "block_rq_insert" then 
		if arr[3] ~= "0" then
		printf("ext_str= %s\n", ky)
		local b0, e0 = string.find(ky, " %(%) ")
		local b1, e1 = string.find(ky, ' %[')
		local offset = ""
		local cmd = ""
		if b0 ~= nil and b1 ~= nil then
			offset = string.sub(ky, e0+1, b1-1)
			cmd    = string.sub(ky, e1+1, string.len(ky)-1)
		end
		printf("dev= %s, typ= %s, len= %s, off= %s, cmd= %s\n",
			arr[1], arr[2], arr[3], offset, cmd)
		lkup = arr[1] .." "..offset
		lkup_tbl[lkup] = {ts, arr[2], arr[3], cmd}
		end
	end
--          <idle>-0     [002] 377143764759: block_rq_complete:    8,0 WS () 298927464 + 384 [0]
	if evt == "block_rq_complete" then 
		local b0, e0 = string.find(ky, " %(%) ")
		local b1, e1 = string.find(ky, ' %[')
		local offset = ""
		local cmd = ""
		if b0 ~= nil and b1 ~= nil then
			offset = string.sub(ky, e0+1, b1-1)
		end
		lkup = arr[1] .." "..offset
		local v = lkup_tbl[lkup]
		if v ~= nil and v[3] ~= "0" then
		--lkup_tbl[lkup] = {arr[2], arr[3], cmd}
		printf("compl dev= %s, typ= %s, len= %s, off= %s, cmd= %s\n",
			arr[1], arr[2], v[3], offset, v[4])
		local new_val = ts - v[1]
		--printf("ext4_da_write in lua= %d\n", new_val)
		-- below assumes ts is in ns
		local ns_factor = 1.0e-9
		new_vals[dura_idx]   = ns_factor * new_val
		new_vals[bytes_idx]  = v[3]
		new_vals[area_idx] = v[4]
		if new_val > 0.0 then
			new_vals[MiBs_idx] = tonumber(v[3]) / (ns_factor * new_val)
		else
			new_vals[MiBs_idx] = 0.0;
		end
		new_vals[emit_idx] = 1
		lkup_tbl[lkup] = nil
		end
	end
	if 1==2 then
	--local action = "unk"
--    jbd2/sda6-8-234   [002] 377123728938: block_rq_insert:      8,0 WS 196608 () 298927464 + 384 [jbd2/sda6-8]
--          <idle>-0     [002] 377143764759: block_rq_complete:    8,0 WS () 298927464 + 384 [0]
	if evt == "block_rq_insert" then 
		local b, e = string.find(ky, " flags ")
		if b == nil then
			error("got problem with "..evt..", str= "..ky)
		end
		ky = string.sub(ky, 1, b)
		b, e = string.find(ky, " len ")
		bytes = tonumber(string.sub(ky, e))
		--printf("ky= '%s', bytes= %d\n", ky, bytes);
	else
		local b, e = string.find(ky, " copied ") -- reads... nah
		if b == nil then
			b, e = string.find(ky, " flags ") -- writes ... nah
			if b == nil then
				error("got problem with "..evt.." str= "..ky)
			end
			--action = "wr"
		else
			--action = "rd"
		end
		ky = string.sub(ky, 1, b)
	end
--  wait.x-17709 [000] 15154023630678: ext4_da_write_begin:  dev 8,6 ino 287900 pos 0 len 6 flags 0
--  wait.x-17709 [000] 15154023637835: ext4_da_write_end:    dev 8,6 ino 287900 pos 0 len 6 copied 6
	if tbl[ky] == nil then
		tbl[ky] = {ts, bytes}
	else
		local new_val = ts - tbl[ky][1]
		--printf("ext4_da_write in lua= %d\n", new_val)
		-- below assumes ts is in ns
		local ns_factor = 1.0e-9
		new_vals[dura_idx]   = ns_factor * new_val
		new_vals[bytes_idx]  = tbl[ky][2]
		new_vals[area_idx] = "system " -- ..action
		if new_val > 0.0 then
			new_vals[MiBs_idx] = tbl[ky][2] / (ns_factor * new_val)
		else
			new_vals[MiBs_idx] = 0.0;
		end
		tbl[ky] = nil
	end
	end
end
