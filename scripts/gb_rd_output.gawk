#<th class='name'>Single-Core Score</th>
#<th class='name'>Multi-Core Score</th>
#<tr class='alt'>
#<td class='name'>
#HDR
#<br>
#</td>
#<td class='score'>;
#<span class='description'></span>
#7885
#<br>
#<span class='description'>28.6 Mpixels/sec</span>

function rd_ofile(infl, sngl, mlti)
{
   tfl = infl;
   nms_idx = 0;
   while ((getline line < tfl) > 0) {
	if (line == "<th class='name'>Single-Core Score</th>") {
		area = sngl;
	}
	if (line == "<th class='name'>Multi-Core Score</th>") {
		area = mlti;
	}
	if (line == "<td class='name'>") {
   		getline line < tfl;
		nm = line;
		#printf("ln= %s %s\n", area, nm);
	}
	if (line == "<td class='score'>") {
   		getline line < tfl;
		if (line == "<span class='description'></span>") {
   			getline line < tfl;
			score = line;
   			getline line < tfl;
			if (line == "<br>") {
   			getline line < tfl;
			beg = index(line, ">");
			end = index(line, "</span>");
			str = substr(line, beg+1, end-beg-1);
			nms_idx++;
			nms[nms_idx][1] = area;
			nms[nms_idx][2] = nm;
			nms[nms_idx][3] = score;
			nms[nms_idx][4] = str;
			nms_idx_arr[area " " nm] = nms_idx;
			printf("%s %s, %s, %s\n", area, nm, score, str);
#<span class='description'>28.6 Mpixels/sec</span>
			}
		}
	}
   }
   close(tl);
}
function get_clocks()
{
 tm_idx = -1;
 clks_str ="./bin/clocks.x";
 a=clks_str;
 tm_lns = -1;
 a | getline tm; tm_ln[++tm_lns] = tm; split(tm, arr); tm_1st = arr[2]+0.0;
 a | getline tm; tm_ln[++tm_lns] = tm; split(tm, arr); tm_raw = arr[2]+0.0;
 a | getline tm; tm_ln[++tm_lns] = tm; split(tm, arr); tm_coarse = arr[2]+0.0;
 a | getline tm; tm_ln[++tm_lns] = tm; split(tm, arr); tm_boot  = arr[2]+0.0;
 a | getline tm; tm_ln[++tm_lns] = tm; split(tm, arr); tm_mono  = arr[2]+0.0;
 a | getline tm; tm_ln[++tm_lns] = tm; split(tm, arr); tm_ofday = arr[2]+0.0;
 close(a);
 printf("tm_ofday= %.9f\n", tm_ofday);
}
BEGIN {
 #printf("odir= %s\n", odir);
 ofile = odir "/gb.html";
 sngl_str = "Single-Core";
 mlti_str = "Multi-Core";
 pd="ps -ef | grep geekbench4 | grep -v grep";
 pd | getline pd_ln; close(pd);
 split(pd_ln, arr);
 gb_pid = arr[2];
 printf("line=  '%s, pid= %s\n", pd_ln, gb_pid);
 kill_aft="JPEG";
 kill_aft="Multi-Core";
 lns_out = -1;
#Uploading results to the Geekbench Browser. This could take a minute or two 
#depending on the speed of your internet connection.
#Upload succeeded. Visit the following link and view your results online:
#  https://browser.geekbench.com/v4/cpu/12212476
#Visit the following link and add this result to your profile:
#  https://browser.geekbench.com/v4/cpu/12212476/claim?key=589037
 get_clocks();
 area = "setup";
 web_pg = "";
 ts_1st = 0;

}
{
	a="date +%s.%N";
	a | getline tm;
	close(a);
	if (ts_1st == 0) {
		ts_1st = tm+0.0;
		printf("ts_1st  = %.9f\n", ts_1st);
	}
	if (area == "setup") {
		ck_area = index($0, sngl_str);
		if (ck_area > 0) {
			area = sngl_str;
		}
	} else if (area == sngl_str) {
		ck_area = index($0, mlti_str);
		if (ck_area > 0) {
			area = mlti_str;
		}
	} else if (area == mlti_str) {
		ck_area = index($0, "Uploading");
		if (ck_area > 0) {
			area = "post";
		}
	}
	if (area == "post" && web_pg == "" && index($0, "http") > 0) {
		web_pg = $1;
	}
	#got_multi= index($0, kill_aft);
	x[++lns_out] = (tm - tm_ofday) + tm_mono;
	y[lns_out] = $0;
	b[lns_out] = area;
	if (area != "setup" && $1 == "Running") {
		bg = index($0, $1) + length($1) + 1;
		bm = substr($0, bg);
		#printf("bm= '%s'\n", bm);
		area_bm = area " " bm;
		tm_idx_arr[area_bm] = lns_out;
		tm_arr[++tm_idx] = area_bm;
	}
	printf("%.9f %s %s\n", x[lns_out], b[lns_out], y[lns_out]);
	if (1==20 && got_multi > 0) {
		cmd = "ls -l /tmp/";
		system(cmd);
		printf("quiting now\n");
		exit(1);
		cmd = "kill -9 " gb_pid;
		printf("try to kill gb_pid %s with cmd: '%s'\n", gb_pid, cmd);
		system(cmd);
	}
}
END {
 #pfay@x86_64_laptop:~/oppat/oppat$ bin/clocks.x 
 #t_first= 2483.209030317
 #t_raw= 2483.232884206
 #t_coarse= 2483.205351649
 #t_boot= 2483.209144629
 #t_mono= 2483.209159488
 #gettimeofday= 1551313934.714109898


 printf("web_pg= %s\n", web_pg);
 if (web_pg != "") {
 	a = "wget " web_pg " -O " ofile;
	system(a);
	#close(a);
 }
 #wget https://browser.geekbench.com/v4/cpu/12212760 -O gb.html
 rd_ofile(ofile, sngl_str, mlti_str);
 #i = nms_idx_arr[mlti_str " " "HDR"];
 #printf("lkup multi HDR= %d: %s %s %s %s\n", i, nms[i][1], nms[i][2], nms[i][3], nms[i][4]);

 phs_file = odir "/gb_phase.tsv";
 for (i=0; i <= tm_idx; i++) {
	area_bm = tm_arr[i]
	j = tm_idx_arr[area_bm];
	tm_beg = x[j];
	tm_end = x[j+1];
	dura = tm_end - tm_beg;
	nms_idx = nms_idx_arr[area_bm];
	printf("%.9f\t%.9f\t%s %s, %s, %s\n", tm_end, dura,
		nms[nms_idx][1], nms[nms_idx][2],
		nms[nms_idx][3], nms[nms_idx][4]) > phs_file;
 }
 #exit 1;
 close(phs_file);

 gb_txt = odir "/gb_stdout.txt";
 for (i=0; i <= lns_out; i++) {
   printf("%.9f %s %s\n", x[i], b[i], y[i]) > gb_txt;
   #printf("ln[%d]= %s\n", i, x[i]);
 }
 close(gb_txt);
 clk_txt = odir "/clocks1.txt";
 for (i=0; i <= tm_lns; i++) {
   printf("%s\n", tm_ln[i]) > clk_txt;
 }
 close(clk_txt);
 cmd = clks_str " > " odir "/clocks2.txt";
 system(cmd);
}
