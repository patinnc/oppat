"use strict";
/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */



var g_parse_delay = 0; // delay 1 ms between parse_data and decompress steps to allow progress info to update
var webSocket;
var messages = document.getElementById("messages");
var chart_divs = [];
var gcanvas_args = [];
var g_charts_done = {cntr:0};
var g_cpu_diagram_flds = null;
var g_got_cpu_diagram_svg = false;
var gmsg_span = null;
var gpixels_high_default = 250;
var doc_title_def = "OPPAT: Open Power/Performance Analysis Tool";
var gjson = {};
var gjson_str_pool = null;
var did_c10_colors = 0;
// the SHAPE_* variable values must agree with the SPARE_* enum in main.cpp
var SHAPE_LINE = 0;
var SHAPE_RECT = 1;
// the IVAL_* variable indices must agree with the json 'ival' order in main.cpp build_shapes_json(build_shapes_json)
var IVAL_SHAPE  = 0;
var IVAL_CPT    = 1;
var IVAL_FE     = 2;
var IVAL_CAT    = 3;
var IVAL_SUBCAT = 4;
var IVAL_PERIOD = 5;
var IVAL_CPU    = 6;
var PTS_X0 = 0;
var PTS_Y0 = 1;
var PTS_X1 = 2;
var PTS_Y1 = 3;
var g_tm_beg = 0.0;
var g_fl_hsh= [];
var g_fl_arr= [];
var g_fl_obj= [];
var g_fl_obj_rt = [];
var g_do_step = true; // connect line chart 'horizontal dashes' with vertical lines if < 3 pixels between end of 1st dash and start of 2nd dash
var g_do_step_changed = false; // set if user overrides default
var FLAMEGRAPH_BASE_CPT = 0; // comm, pid, tid
var FLAMEGRAPH_BASE_CP  = 1; // comm, pid
var FLAMEGRAPH_BASE_C   = 2; // comm
var g_flamegraph_base = FLAMEGRAPH_BASE_CPT;
var did_prt = 0;
var g_svg_obj = {status:0, str:""};

var chart_did_image = [];
var gcolor_def = "lavender"; // lavender e6e6fa for events above 'number of colors' rank
var gcolor_lst = ["#b0556a", "#7adf39", "#8d40d6", "#ead12d", "#0160eb", "#aaed78", "#f945b7", "#04e6a0", "#cf193b", "#4df8ca", "#b21f72", "#41981b", "#b773eb", "#276718", "#f39afb", "#0ea26a", "#015fc6", "#ec7118", "#108cf5", "#feab4f", "#1eacf8", "#a13502", "#49f6fd", "#9e5d33", "#30d8ec", "#ab952f", "#8156a5", "#f5db82", "#1e67a9", "#f6b17c", "#47caf9", "#695909", "#7daaef", "#a4ce84", "#ef89bb", "#1c6c43", "#ecb5f2", "#7ddab8", "#0f88b4", "#07a1b2"];
var number_of_colors = gcolor_lst.length;

var mymodal = null;
var mymodal_span = null;
var mymodal_span_text = null;

/* Adds Element BEFORE NeighborElement */
Element.prototype.appendBefore = function(element) {
	element.parentNode.insertBefore(this, element);
}, false;

/* Adds Element AFTER NeighborElement */
Element.prototype.appendAfter = function(element) {
	element.parentNode.insertBefore(this, element.nextSibling);
}, false;


function clearToolTipText(tt_ele) {
	tt_ele.style.borderWidth = '0px';
	tt_ele.setAttribute("visibility", 'hidden');
	tt_ele.setAttribute("opacity", '0');
	tt_ele.innerHTML = '';
}

function setTooltipText(a_tooltip, a_canvas, current_text, x, y) {
	a_tooltip.style.borderWidth = '1px';
	a_tooltip.innerHTML = current_text;
	a_tooltip.setAttribute("visibility", 'visible');
	a_tooltip.setAttribute("display", 'inline-block');
	if (x > (0.5 * a_canvas.width)) {
		a_tooltip.style.removeProperty('left');
		a_tooltip.style.right = (a_canvas.width - x + 20) + 'px';
	} else {
		a_tooltip.style.removeProperty('right');
		a_tooltip.style.left = (x + 20) + 'px';
	}
	a_tooltip.style.removeProperty('bottom');
	a_tooltip.style.top = (y + 20) + 'px';
	let visible = checkVisible(a_tooltip);
	if (!visible) {
		//tooltip_top_bot = 'top';
		a_tooltip.style.removeProperty('top');
		a_tooltip.style.bottom = (a_canvas.height - y - 20) + 'px';
	}
	console.log("did setToolTip");
}

function checkVisible(elm) {
	let rect = elm.getBoundingClientRect();
	let viewHeight = Math.max(document.documentElement.clientHeight, window.innerHeight);
	//console.log("rect.top= "+rect.top+", bot= "+rect.bottom+", height= "+viewHeight);
	//console.log(rect);
	if (rect.bottom > viewHeight) {
		return false;
	}

	return !((rect.bottom < 0) || ((rect.top - viewHeight) >= 0));
}

function myBarMove(cur_val, max_val) {
  var elem = document.getElementById("myBar");
  var width = 100 * cur_val/max_val;
  elem.style.width = width + '%';
  elem.innerHTML = width * 1  + '%';
  //var id = setInterval(frame, 10);
  /*
  function frame() {
    if (width >= 100) {
      clearInterval(id);
    } else {
      width++;
      elem.style.width = width + '%';
      elem.innerHTML = width * 1  + '%';
    }
  }
  */
}

function decompress_str(typ_data, str)
{
    update_status("websocket opened. ck tab title for status msgs");
	let txt = "inflate "+typ_data;
	document.title = txt;
	mymodal.style.display = "none";
	mymodal_span_text.innerHTML = document.title;
	let tm_now = performance.now();
	update_status(txt);
	mymodal.style.display = "block";
	console.log(txt);
	let tm_beg = performance.now();

	let ostr = JXG.decompress(str);
	let tm_end = performance.now();
	let tm_str = tm_diff_str(0.001*(tm_end-tm_beg), 3, "secs");
	txt = "inflated "+typ_data+" tm= "+tm_str;
	if (typ_data == 'str_pool') {
		txt += ", now start decompressing chart_data. This can take 1-30 seconds.";
	} else {
		txt += ", now start parsing chart_data, see tab title for updates";
	}
	console.log(txt);
	mymodal.style.display = "none";
	document.title = txt;
	update_status(txt);
	mymodal_span_text.innerHTML = document.title;
	mymodal.style.display = "block";

	console.log("decompress len= "+ostr.length);
	if (ostr.length > 20) {
		console.log("nw_str= "+ostr.substr(0, 20));
	}
	return ostr;
}


function update_status(txt)
{
	if (gmsg_span == null) {
		gmsg_span = document.getElementById("msg_span");
	}
	let tm_now = performance.now();
	let tm_diff_secs = 0.001 * (tm_now - g_tm_beg);
	let tm_str = tm_diff_str(tm_diff_secs, 3, "secs");
	let txt2 = txt+", elapsed= "+tm_str;
	//gmsg_span.innerHTML = "";
	//gmsg_span.textContent = "<b>"+txt2+"</b>";
	gmsg_span.innerHTML   = txt2;
	//gmsg_span.style.visibility = 'hidden';
	//gmsg_span.style.visibility = 'visible';
	//gmsg_span.style.display = 'block';
	    			//clr_button.style.visibility = 'visible';
	//document.getElementById('parentOfElementToBeRedrawn').style.display = 'block';
	//gmsg_span.innerHTML = txt2;
	//$('#msg_span').text(txt2);
	//console.log(txt2);
}

function ck_cmd(str_in, str_in_len, ck_str) {
	var ck_str_len  = ck_str.length;
	if (str_in_len >= ck_str_len && ck_str == str_in.substring(0, ck_str_len)) {
		return ck_str_len;
	}
	return 0;
}

var decod = null;
if (!("TextDecoder" in window)) {
	console.log("Sorry, this browser does not support TextDecoder...");
} else {
	decod = new TextDecoder("utf-8");
}

var gsync_zoom_linked = false;
var gsync_zoom_redrawn_charts = {cntr:0, cpu_diag_redraw:0};
var gsync_zoom_redrawn_charts_map = [];
var gsync_zoom_active_now = false;
var gsync_zoom_active_redraw_beg_tm = 0;
var gsync_zoom_charts_redrawn = 0;
var gsync_zoom_charts_hash = {}
var gsync_zoom_arr = [];
var gsync_zoom_last_zoom = {chrt_idx:-1, x0:0.0, x1:0.0, abs_x0:0.0, abs_x1:0.0};
var gsync_text = "Zoom/Pan: Zoom all to last";
var gLinkZoom_iter = 0;

function set_zoom_all_charts(j, need_tag)
{
		//let j = gsync_zoom_last_zoom.chrt_idx;
		//let need_tag = gjson.chart_data[j].file_tag
		gLinkZoom_iter++;
		if (j >= 0) {
			console.log(sprintf("lnk_zm[%d]: x0= %f x1= %f need_tag= %s", j, gcanvas_args[j][6], gcanvas_args[j][7], need_tag));
		}
		if (gsync_zoom_arr.length == 0) {
			for (let i=0; i < gjson.chart_data.length; i++) {
				gsync_zoom_arr.push({iter:-1, x0:-1.0, x1:0.0, file_tag:gjson.chart_data[i].file_tag});
			}
		}
		gsync_zoom_redrawn_charts_map.length = gjson.chart_data.length;
		for (let i=0; i < gjson.chart_data.length; i++) {
			gsync_zoom_redrawn_charts_map[i] = 0;
		}

		let need_to_redraw = 0;
		for (let i=0; i < gjson.chart_data.length; i++) {
			if (gjson.chart_data[i].file_tag != need_tag) {
				continue;
			}
			need_to_redraw++;
			//el.textContent = "Zoom/Pan: zooming i="+i;
			let t0      = gjson.chart_data[i].ts_initial.ts;
			let zoom_x0 = gjson.chart_data[i].x_range.min;
			let zoom_x1 = gjson.chart_data[i].x_range.max;
			let tabs_x0 = t0 + zoom_x0;
			let tabs_x1 = t0 + zoom_x1;
			if (tabs_x0 < gsync_zoom_last_zoom.abs_x0) {
				tabs_x0 = gsync_zoom_last_zoom.abs_x0;
			}
			if (tabs_x1 > gsync_zoom_last_zoom.abs_x1) {
				tabs_x1 = gsync_zoom_last_zoom.abs_x1;
			}
			gsync_zoom_arr[i].x0 = tabs_x0;
			gsync_zoom_arr[i].x1 = tabs_x1;
			gsync_zoom_arr[i].iter = gLinkZoom_iter;
			tabs_x0 -= t0;
			tabs_x1 -= t0;
			//console.log("Zooom ii= "+i);
		}
		gsync_zoom_redrawn_charts.cntr = 0;
		gsync_zoom_redrawn_charts.need_to_redraw = need_to_redraw;
		gsync_zoom_linked = true;
}

function LinkZoom( el, cb )
{
	let ckd = cb.checked;
	console.log("zoom_ck= "+ckd);
	if (el.textContent === gsync_text) {
		console.log(sprintf("lnk_zm: x0= %f x1= %f", gsync_zoom_last_zoom.x0, gsync_zoom_last_zoom.x1));

		let j = gsync_zoom_last_zoom.chrt_idx;
		set_zoom_all_charts(gsync_zoom_last_zoom.chrt_idx, gjson.chart_data[j].file_tag);
		let jj=0;
		function myDelay () {           //  create a loop function
			setTimeout(function () {    //  call a 3s setTimeout when the loop is called
				console.log('jj= '+jj);          //  your code here
				jj++;                     //  increment the counter
				if (gsync_zoom_redrawn_charts.cntr < gsync_zoom_redrawn_charts.need_to_redraw) {
					myDelay();             //  ..  again which will trigger another 
				} else {
					parse_svg();
					console.log("++++g_charts_done aft= "+g_charts_done.cntr);
					console.log("__end LinkZoom()");
				}
			}, 1000)
		}
		//abcd
		myDelay();
		console.log("Zooom all done");
		el.textContent = gsync_text;
		cb.checked = true;
	} else {
		el.textContent = "Zoom/Pan: UnLinked";
		gsync_zoom_linked = false;
	}
	console.log("gsync_zoom_linked= "+ gsync_zoom_linked);

}

var lhs_menu_ch_list_state = 0;
var lhs_menu_ch_list = [];
var lhs_menu_nm_list = [];

function menu_hover(menu_i)
{
	let i = menu_i;
	let ch_idx = lhs_menu_ch_list[i].ch_idx;
	let txt = gjson.chart_data[ch_idx].title;
	let ele_nm = lhs_menu_ch_list[i].hvr_nm;
	let div_nm = lhs_menu_ch_list[i].div_nm;
	let ele = document.getElementById(ele_nm);
	if (ele != null) {
		//console.log("try to scrooll to "+ele_nm);
		ele.scrollIntoView(true);
	}
	//console.log("hovered over menu item= "+i+", ttl= "+txt+", elenm= "+ele_nm+", ele="+ele+",div_nm="+div_nm);
}

function ele_show_hide(menu_idx, show_me)
{
	if (menu_idx < 0 || menu_idx >= lhs_menu_ch_list.length) {
		console.log("ele_show_hide prob: menu_idx= "+menu_idx+", lhs_menu_ch_list.length= "+lhs_menu_ch_list.length);
		return;
	}
	let ele_nm = lhs_menu_ch_list[menu_idx].hvr_nm;
	let ele = document.getElementById(ele_nm);
	if (ele != null) {
		if (show_me) {
			ele.style.display = "block";
			lhs_menu_ch_list[menu_idx].display_state = "show";
		} else {
			ele.style.display = "none";
			lhs_menu_ch_list[menu_idx].display_state = "hide";
		}
	}
}

function change_pixels_high(cb, menu_idx, nm_idx)
{
	console.log("text val:"+cb.value);
	let val = +cb.value;
	if (val >= 100 && val <= 1500) {
		gpixels_high_default = val;
		console.log("set gpixels_high_default= "+gpixels_high_default);
		update_status("start redraw charts");
		for (let i=0; i < gjson.chart_data.length; i++) {
			can_shape(gcanvas_args[i][0], gcanvas_args[i][1], gcanvas_args[i][2], gcanvas_args[i][3], gcanvas_args[i][4], gcanvas_args[i][5], gcanvas_args[i][6], gcanvas_args[i][7], gcanvas_args[i][8], gcanvas_args[i][9]);
		}
	}
	return;
}

function lhs_menu_change(cb, menu_idx, nm_idx)
{
	console.log("checked:"+cb.checked+",indeter:"+cb.indeterminate+",nm_idx= "+nm_idx);
	let ckd = cb.checked;
	let ind = cb.indeterminate;
	let loop_over_all = false;
	let loop_idxs = [];
	if (nm_idx == -4) {
		let nm  = 'lhs_menu_root_linkzoom_label';
		let ele = document.getElementById(nm);
		LinkZoom(ele, cb);
		return;
	}
	if (nm_idx == -3) {
		let nm  = 'lhs_menu_root_fg_label';
		let me_ele = document.getElementById(nm);
		let fg_base = g_flamegraph_base;
		let txt = "";
		if (fg_base == FLAMEGRAPH_BASE_CPT) {
			txt = "Flamegraph: by comm, pid";
			g_flamegraph_base = FLAMEGRAPH_BASE_CP;
			cb.checked = false;
			cb.indeterminate = true;
		} else if (fg_base == FLAMEGRAPH_BASE_CP) {
			txt = "Flamegraph: by comm";
			g_flamegraph_base = FLAMEGRAPH_BASE_C;
			cb.checked = false;
			cb.indeterminate = false;
		} else if (fg_base == FLAMEGRAPH_BASE_C) {
			txt = "Flamegraph: by comm, pid, tid";
			g_flamegraph_base = FLAMEGRAPH_BASE_CPT;
			cb.checked = true;
			cb.indeterminate = false;
		}
		me_ele.textContent = txt;
		console.log("clicked flamegraph base to "+ txt);
		return;
	}
	if (nm_idx == -2) {
		console.log("clicked connect lines state= "+ ckd + ", ind= "+ind);
		g_do_step = ckd;
		g_do_step_changed = true;
		return;
	}
	if (nm_idx == -1) {
		console.log("clicked all charts: state= "+ ckd + ", ind= "+ind);
		loop_over_all = true;
		cb.indeterminate = false;
		ind = false;
		for (let i=0; i < lhs_menu_nm_list.length; i++) {
			let kids = lhs_menu_nm_list[i].kids;
			if (kids.length > 0) {
				loop_idxs.push(i);
			}
		}
	} else {
		loop_idxs.push(nm_idx);
	}
	for (let lp=0; lp < loop_idxs.length; lp++) {
		nm_idx = loop_idxs[lp];
		let me_nm = lhs_menu_nm_list[nm_idx].nm;
		let me_ele = document.getElementById(me_nm);
		//console.log("me_nm= "+me_nm+", me_ele= "+me_ele);
		me_ele.checked = ckd;
		me_ele.indeterminate = false;
		let kids = lhs_menu_nm_list[nm_idx].kids;
		if (kids.length > 0) {
			cb.indeterminate = false;
			// so we have kid cbs
			for (let i=0; i < kids.length; i++) {
				let j = kids[i];
				let nm = lhs_menu_nm_list[j].nm;
				let ele = document.getElementById(nm);
				ele.checked = ckd;
				let menu_idx = lhs_menu_nm_list[j].menu_i;
				ele_show_hide(menu_idx, ckd);
			}
		} else {
			let menu_idx = lhs_menu_nm_list[nm_idx].menu_i;
			ele_show_hide(menu_idx, ckd);
			let dad_idx = lhs_menu_nm_list[nm_idx].dad;
			//console.log("dad_idx="+dad_idx);
			let kids = lhs_menu_nm_list[dad_idx].kids;
			let dad_nm = lhs_menu_nm_list[dad_idx].nm;
			let dad_ele = document.getElementById(dad_nm);
			let all_same = true;
			for (let i=0; i < kids.length; i++) {
				let j = kids[i];
				let nm = lhs_menu_nm_list[j].nm;
				let ele = document.getElementById(nm);
				if (ele.checked != ckd) {
					all_same = false;
					break;
				}
			}
			dad_ele.indeterminate = !all_same;
			if (all_same) {
				dad_ele.checked       = ckd;
				//dad_ele.indeterminate = false;
			} else {
				dad_ele.checked       = false;
			}
			console.log("dad.ckd= "+dad_ele.checked+",dad.ind= "+dad_ele.indeterminate, );
		}
	}
	return;
}

function lhs_menu_click(e)
{
	console.log(e);
	return;
	console.log("readOnly:"+cb.readOnly+", checked:"+cb.checked+",indeter:"+cb.indeterminate);
	let i = menu_i;
	let ch_idx = lhs_menu_ch_list[i].ch_idx;
	let txt = gjson.chart_data[ch_idx].title;
	let ele_nm = lhs_menu_ch_list[i].hvr_nm;
	let ele = document.getElementById(ele_nm);
	let ckd = ele.value;
	let ind = ele.indeterminate;
	console.log("clicked over menu item= "+i+", ttl= "+txt+", elenm= "+ele_nm+", ele="+ele+", ckd="+ckd);
}

function can_shape(chrt_idx, use_div, chart_data, tm_beg, hvr_clr, px_high_in, zoom_x0, zoom_x1, zoom_y0, zoom_y1) {
	if (typeof chart_data == 'undefined') {
		return;
	}
	if ( typeof can_shape == 'undefined' ) {
		let can_shape = {};
	if ( typeof can_shape.sv_pt_prv == 'undefined' ) {
		can_shape.sv_pt_prv = null;
		can_shape.sv_pt     = null;
		can_shape.hvr_arr = [];
	}
	}
	if (typeof can_shape.invocation_num == 'undefined') {
		can_shape.invocation_num = 0;
	}
	can_shape.invocation_num++;
	let px_high = px_high_in;
	if (typeof px_high == 'undefined' || px_high < 100) {
		px_high = gpixels_high_default;
		//console.log("___set ch1 px_high= "+px_high);
	} else {
		//console.log("___set ch2 px_high= "+px_high);
	}
	var xPadding = 50;
	var yPadding = 50;

	let file_tag_idx = chart_data.file_tag_idx;
	while(file_tag_idx >= g_fl_obj.length) {
		g_fl_obj.push({});
		g_fl_obj_rt.push({});
		g_fl_hsh.push({});
		g_fl_arr.push([]);
	}
	let tm_top0= performance.now();
	//console.log("inside0: hvr_clr= "+hvr_clr+", minx= "+zoom_x0+", maxx= "+zoom_x1+", zoom_y0= "+zoom_y0+", zoom_y1= "+zoom_y1+", chart_data.x_range.max= "+chart_data.x_range.max);
	let win_sz = get_win_wide_high();
	let x_mar = 30; // leave room for vert scroll bar
	let px_wide = win_sz.width - x_mar;
	//console.log("px_wide = "+px_wide);
	let ch_type = chart_data.chart_type;

	let chrt_div = document.getElementById(use_div);
	if (chrt_div == null) {
		addElement ('div', use_div, 'chart_anchor', 'before');
		chrt_div = document.getElementById(use_div);
	}
	let minx, maxx, miny, maxy, sldr_cur;
	let draw_mini_box = {};
	let draw_mini_cursor_prev = null;
	let mycanvas2_ctx = null;
	let tot_line = {};
	tot_line.evt_str = [];
	tot_line.xarray  = [];
	tot_line.yarray  = [];
	tot_line.xarray2 = [];
	tot_line.yarray2 = [];
	tot_line.evt_str_base_val_arr = [];
	if (chart_data.tot_line != "") {
		//if (chart_data.chart_tag == "CYCLES_PER_UOP_port_0_CHART")
		if (typeof chart_data.tot_line_opts_xform !== "undefined" &&
			chart_data.tot_line_opts_xform == "map_cpu_2_core") {
			let u_hsh = {};
			let u_arr = [];
			for (let i=0; i < chart_data.map_cpu_2_core.length; i++) {
				if (typeof u_hsh[chart_data.map_cpu_2_core[i]] == 'undefined') {
					u_hsh[chart_data.map_cpu_2_core[i]] = i;
					u_arr.push(chart_data.map_cpu_2_core[i]);
				}
			}
			u_arr.sort(function(a, b){return a - b});
			for (let i=0; i < u_arr.length; i++) {
				let str = sprintf("%d", u_arr[i]);
				tot_line.evt_str_base_val_arr.push(u_arr[i]);
				if (typeof chart_data.tot_line_opts_yvar_fmt !== 'undefined') {
					str = sprintf(chart_data.tot_line_opts_yvar_fmt, u_arr[i]);
				}
				tot_line.evt_str.push(str);
				tot_line.yarray.push([]);
				tot_line.yarray2.push([]);
				tot_line.xarray2.push([]);
			}
		} else {
			tot_line.evt_str.push(chart_data.tot_line);
			tot_line.yarray.push([]);
			tot_line.yarray2.push([]);
			tot_line.xarray2.push([]);
		}
	}
	tot_line.subcat_rng_idx = {};
	tot_line.subcat_rng_arr = [];
	tot_line.event_list_idx = [];
	//console.log("tot_line.evt_str= '"+tot_line.evt_str+"'");

	function reset_minx_maxx(zm_x0, zm_x1, zm_y0, zm_y1) {
		if (gsync_zoom_linked) {
			minx = zm_x0;
			maxx = zm_x1;
			//console.log("reset_minx: set minx= "+minx+", maxx= "+maxx+", title= "+chart_data.title);
		} else {
			if (zm_x0 > chart_data.x_range.min) {
				minx = zm_x0;
			} else {
				minx = chart_data.x_range.min;
			}
			if (zm_x1 < chart_data.x_range.max) {
				maxx = zm_x1;
			} else {
				maxx = chart_data.x_range.max;
			}
		}
		if (zm_y0 > chart_data.y_range.min) {
			miny = zm_y0;
		} else {
			miny = chart_data.y_range.min;
		}
		if (zm_y1 < chart_data.y_range.max) {
			maxy = zm_y1;
		} else {
			maxy = chart_data.y_range.max;
		}
		gsync_zoom_last_zoom.chrt_idx = chrt_idx;
		gsync_zoom_last_zoom.x0 = minx;
		gsync_zoom_last_zoom.x1 = maxx;
		gsync_zoom_last_zoom.abs_x0 = minx + chart_data.ts_initial.ts;
		gsync_zoom_last_zoom.abs_x1 = maxx + chart_data.ts_initial.ts;
		//console.log(sprintf("zm.x0= %f, zm.x1= %f", minx, maxx));
		if (mycanvas2_ctx != null) {
			/*
			let xd = (chart_data.x_range.max - chart_data.x_range.min);
			let xd0 = 0.5 * (maxx - minx);
			let xd1 = minx + xd0;
			let xd2 = (xd1) / xd;
			*/
			let clp_beg = chart_data.ts_initial.tm_beg_offset_due_to_clip;
			let xd = (chart_data.x_range.max - clp_beg);
			let xd0 = 0.5 * (maxx - minx);
			let xd1 = minx - clp_beg + xd0;
			let xd2 = (xd1) / xd;
			//let xtxt = "__draw_mini: mnx="+minx+",mxx= "+maxx+", chmn= "+chart_data.x_range.min+", chmx= "+chart_data.x_range.max+", xd2= "+xd2+", xd0= "+xd0+", num= "+xd1+", den= "+xd+", mxx-mnx= "+(maxx-minx)+"<br>(minx + (0.5 * (maxx-minx))) / (chart_data.x_range.max - chart_data.x_range.min)<br>("+minx+" + (0.5 * ("+maxx+ " - " +minx+"))) / ("+chart_data.x_range.max+ " - " + chart_data.x_range.min+")<br>("+minx+" + (0.5 * ("+(maxx-minx)+"))) / ("+(chart_data.x_range.max-chart_data.x_range.min)+")";
			//set_chart_text(myhvr_clr_txt, myhvr_clr_txt_btn, xtxt);
			//console.log("__draw_mini: mnx="+minx+",mxx= "+maxx+", chmn= "+chart_data.x_range.min+", chmx= "+chart_data.x_range.max+", xd2= "+xd2+", xd0= "+xd0+", num= "+xd1+", den= "+xd+", mxx-mnx= "+(maxx-minx));
			if (isNaN(xd2)) {
				console.log("cd.xr.mx= "+chart_data.x_range.max+", mn="+chart_data.x_range.min);
			} else {
				draw_mini(xd2, "a");
			}
		}
	}


	//console.log("sldr_cur= "+sldr_cur);
	let myhvr_clr = document.getElementById(hvr_clr);
	let mycanvas_nm_title = "title_"+hvr_clr;
	//myhvr_clr = document.getElementById(hvr_clr);
	if (myhvr_clr == null) {
		addElement ('div', hvr_clr, 'chart_anchor', 'before');
		addElement ('div', hvr_clr+'_bottom', 'chart_anchor', 'before');
		//console.log("set sldr_cur= "+sldr_cur);
		myhvr_clr = document.getElementById(hvr_clr);
		let str ='<div class="center-outer-div"><div class="center-inner-div" id="'+mycanvas_nm_title+'"></div></div><div class="tooltip"><canvas id="canvas_'+hvr_clr+'" width="'+(px_wide-2)+'" height="'+(px_high-4)+'" style="border:1px solid #000000;"></canvas><span class="tooltiptext" id="tooltip_'+hvr_clr+'"></span></div><canvas id="canvas2_'+hvr_clr+'" width="'+(px_wide-2)+'" height="25px" style="border:1px solid #000000;"></canvas><div id="after_canvas_'+hvr_clr+'" style="display:inline-block"><button style="display:inline-block" onclick="showLegend(\''+hvr_clr+'\', \'show_top_20\');" />Legend top20</button><button style="display:inline-block" onclick="showLegend(\''+hvr_clr+'\', \'show_all\');" />Legend all</button><button style="display:inline-block" onclick="showLegend(\''+hvr_clr+'\', \'hide_all\');" />Legend hide</button><span id="'+hvr_clr+'_legend" style="display:inline-block;height:200px;word-wrap: break-word;overflow-y: auto;"></span><button id="but_' + hvr_clr +'" class="clrTxtButton" style="display:inline-block;visibility:hidden" onclick="clearHoverInfo(\''+hvr_clr+'_txt\', \''+hvr_clr+'\');" />Clear_text</button><span id="'+hvr_clr+'_txt" style="margin-left:0px;" clrd="n" ></span></div><span id="'+hvr_clr+'_canspan"></span><hr>';
		//console.log("create hvr_clr butn str= "+str);
		myhvr_clr.innerHTML = str;
	}
	let mytooltip      = document.getElementById("tooltip_"+hvr_clr);
	let myhvr_clr_txt = document.getElementById(hvr_clr+'_txt');
	let myhvr_clr_txt_btn = document.getElementById('but_'+hvr_clr);
	let mycanvas_title = document.getElementById(mycanvas_nm_title);
	let ch_title = chart_data.title;
	let file_tag = chart_data.file_tag;
	if (file_tag.length > 0) {
		ch_title = '<b>'+file_tag+'</b>&nbsp'+ch_title;
	}
	mycanvas_title.innerHTML = ch_title;
	let mycanvas = document.getElementById('canvas_'+hvr_clr);
	if (mycanvas != null && mycanvas.height != (px_high-4)) {
		mycanvas.height = px_high-4;
	}
	let canvas3_px_high = mycanvas.height;
	let mycanvas2 = document.getElementById('canvas2_'+hvr_clr);
	if (chart_data.x_range.max == 0 && chart_data.x_range.min == 0) {
		mycanvas_title.innerHTML = ch_title + "<br> no data for chart";
		mycanvas.height = 0;
		mycanvas2.height = 0;
		let myaftercanvas = document.getElementById('after_canvas_'+hvr_clr);
		myaftercanvas.style.display = "none";
		mycanvas.style.display = "none";
		mycanvas2.style.display = "none";
		mytooltip.style.display = "none";
		let mycanspan = document.getElementById(hvr_clr+'_canspan');
		mycanspan.style.display = "none";
		return;
	}
	let mylegend  = document.getElementById(hvr_clr+'_legend');
	let tm_here_01 = performance.now();
	let proc_select = {};
	let build_flame_rpt_timeout = null;

	var zoom_ckr = new TaskTimer(1000);

	reset_minx_maxx(zoom_x0, zoom_x1, zoom_y0, zoom_y1);

	zoom_ckr.addTask({
	    name: 'zoom_ck '+hvr_clr,       // unique name of the task
	    tickInterval: 2,    // run every 5 ticks (5 x interval = 5000 ms)
	    totalRuns: 0,      // run 10 times only. (set to 0 for unlimited times)
	    zoom_iter_last: -1,      // run 10 times only. (set to 0 for unlimited times)
	    callback: function (task) {
		// code to be executed on each run
		//if (gsync_zoom_linked && !gsync_zoom_active_now)
		if (gsync_zoom_linked) {
			let did_grph=0;
			let i = chrt_idx;
			if (gsync_zoom_arr[i].iter > task.zoom_iter_last) {
				//console.log('task :'+task.name + ' id= '+task.zoom_iter_last);
				task.zoom_iter_last = gsync_zoom_arr[i].iter;
				let x0 = gsync_zoom_arr[i].x0 - chart_data.ts_initial.ts;
				let x1 = gsync_zoom_arr[i].x1 - chart_data.ts_initial.ts;
				if (typeof gsync_zoom_charts_hash[chrt_idx] == 'undefined') {
					gsync_zoom_charts_hash[chrt_idx] = {tms:0, zoom_iter:task.zoom_iter_last, x0:x0, x1:x1}
				}
				gsync_zoom_charts_hash[chrt_idx].tms++;
				//if (gsync_zoom_charts_hash[chrt_idx].tms == 1 ||
				//	x0 != gsync_zoom_charts_hash[chrt_idx].x0 || x1 != gsync_zoom_charts_hash[chrt_idx].x1) {
				if (1==20) {
				console.log("z_h["+chrt_idx+"].tms= "+ gsync_zoom_charts_hash[chrt_idx].tms+", zido= "+
					task.zoom_iter_last+",n= "+gsync_zoom_charts_hash[chrt_idx].zoom_iter+"xo= "+
					gsync_zoom_charts_hash[chrt_idx].x0+"-"+gsync_zoom_charts_hash[chrt_idx].x1+", xn= "+
					x0+"-"+x1
					);
				}
					gsync_zoom_charts_hash[chrt_idx].x0 = x0;
					gsync_zoom_charts_hash[chrt_idx].x1 = x1;
					gsync_zoom_charts_redrawn++;
					let tm_now  = performance.now();
					//document.title = "upd grf "+gsync_zoom_charts_redrawn+",tm="+
					//	tm_diff_str(0.001*(tm_now-gsync_zoom_active_redraw_beg_tm), 1, "secs");
					zoom_to_new_xrange(x0, x1, false);
				//}
			}
			//document.title = doc_title_def;
		}
	    }
	});

	// Start the timer
	zoom_ckr.start();


	function my_wheel_start2(canvas) {
		// based on https://jsfiddle.net/rafaylik/sLjyyfox/
		var firstTouch, lastTouch;
		var firstTime;
		var elem = canvas,
		    info = myhvr_clr_txt,
		    marker = true,
		    delta,
		    delta_x, delta_y,
		    direction,
		    interval = 200,
		    counter1 = 0,
		    counter2;

		if (elem.addEventListener) {
		  if ('onwheel' in document)            elem.addEventListener('wheel',wheel);
		  else if ('onmousewheel' in document)  elem.addEventListener('mousewheel',wheel);
		  else                                  elem.addEventListener('MozMousePixelScroll',wheel);
		} else                                  elem.attachEvent('onmousewheel',wheel);

		function wheel(e){
		  e = e||window.event;
		  let tch_tst = e;
		  let pt_tst = get_xy(canvas, tch_tst);
		  //console.log("wheel: y= "+(pt_tst.y - maxy));
		  if ((pt_tst.x - minx) < 0.0 || (pt_tst.y - maxy) > 0.0) {
		     // if we aren't in the actual chart viewport then don't zoom
		     return;
		  }
		  counter1++;
		  e.preventDefault();
		  delta = e.deltaY||e.detail||e.wheelDelta;
		  if (delta>0) {direction = 'up';} else {direction = 'down';}
		  if (marker) {
		    firstTouch = e;
		    delta_x = e.deltaX;
		    delta_y = e.deltaY;
		    //console.log(e);
		    wheelStart(e);
		  }
		  delta_x += e.deltaX;
		  delta_y += e.deltaY;
		  //lastTouch = e;
		  return false;
		}
		function wheelStart(){
		  marker = false;
		  gsync_zoom_active_redraw_beg_tm = performance.now();
		  gsync_zoom_active_now = true;
		  wheelAct();
		  //info.innerHTML = 'event start: '+direction;
		}
		function wheelAct(){
		  counter2 = counter1;
		  setTimeout(function(){
			let pt0 = get_xy(canvas, firstTouch);
			let use_x = false;
			let x_addr = delta_y;
		    if (Math.abs(delta_x) >= Math.abs(delta_y)) {
			use_x = true;
			x_addr = delta_x;
		    }
		    if (delta_x != 0.0 || delta_y != 0.0) {
			let mycli = { clientX: (firstTouch.clientX-x_addr), clienty: firstTouch.clientY};
			let pt1 = get_xy(canvas, mycli);
			let xdiff = pt1.x - pt0.x;
		    if (use_x) {
				// change " - delta_x " to " + delta_x" below to change dir for horizontal scroll in response to mouse wheel
				let nx0 = minx - xdiff;
				let nx1 = maxx - xdiff;
				//console.log("new wheel pan x: "+nx0+" - "+nx1+", xdiff= "+xdiff+", delta_x= "+delta_x+", pt0.x= "+pt0.x+", pt1.x= "+pt1.x);
				zoom_to_new_xrange(nx0, nx1, true);
		    } else {
				let nx0 = minx - 0.5 * xdiff;
				let nx1 = maxx + 0.5 * xdiff;
				//console.log("new wheel zoom x: "+nx0+" - "+nx1+", xdiff= "+xdiff+", delta_x= "+delta_x+", pt0.x= "+pt0.x+", pt1.x= "+pt1.x);
				zoom_to_new_xrange(nx0, nx1, true);
		    }
		    }
		    delta_x = 0.0;
		    delta_y = 0.0;
		    if (counter2 == counter1) {
		      wheelEnd();
		    } else {
		      wheelAct();
		      //info.innerHTML = info.innerHTML+'<br>...';
		    }
		  },interval);
		}
		function wheelEnd(){
			marker = true,
			gsync_zoom_charts_redrawn = 0;
			gsync_zoom_charts_hash = {};
			gsync_zoom_active_now = false;
			counter1 = 0,
			counter2 = 0;
			//info.innerHTML = info.innerHTML+'<br>event end'+", x_chg= "+delta_x+", y_chg="+delta_y;
			delta_x = 0;
			delta_y = 0;
		}
	}
	function get_xy(canvas, tch) {
		let rect = canvas.getBoundingClientRect(),
			x = Math.trunc(tch.clientX - rect.left - xPadding),
			y = Math.trunc(tch.clientY - rect.top);
		let nx1 = 1.0 * x;
		let ny1 = 1.0 * y;
		let xdiff1= +nx1 / (px_wide - xPadding);
		let ydiff1= +ny1 / (canvas_px_high(null) - yPadding);
		let nx0 = +minx+( xdiff1 * (maxx - minx));
		let ny0 = +miny+( ydiff1 * (maxy - miny));
		return {x: nx0, y: ny0};
	}
/*
*/
	function my_touch_start(canvas) {
		// from http://bencentra.com/code/2014/12/05/html5-canvas-touch-events.html
		// Prevent scrolling when touching the canvas
/*
*/
		var mousePos = { x:0, y:0 };
		var lastPos = mousePos;
		var firstTouch, lastTouch;
		var firstTime;
		// Set up touch events for mobile, etc
		canvas.addEventListener("touchstart", function (e) {
		  mousePos = getTouchPos(canvas, e);
		  firstTime = performance.now();
		  let touch = e.touches[0];
		  firstTouch = touch;
		  lastTouch = touch;
		  console.log("touch begin x= "+touch.clientX+", y= "+touch.clientY);
		  return;
		  let mouseEvent = new MouseEvent("mousedown", {
		    clientX: touch.clientX,
		    clientY: touch.clientY
		  });
		  //canvas.dispatchEvent(mouseEvent);
		}, false);
		canvas.addEventListener("touchend", function (e) {
		  //let mouseEvent = new MouseEvent("mouseup", {});
		  //let touch = e.touches[0];
			let pt0 = get_xy(canvas, firstTouch);
			let pt1 = get_xy(canvas, lastTouch);
			let nx0 = pt0.x;
			let nx1 = pt1.x;
    			console.log("pan nx0= "+nx0+", nx1= "+nx1);
			let diff = maxx - minx;
			if (diff > 0) {
				zoom_to_new_xrange(minx - (nx1 - nx0), maxx - (nx1 - nx0), true);
			}
			return;
		}, false);
		canvas.addEventListener("touchmove", function (e) {
		  let touch = e.touches[0];
		  lastTouch = touch;
		  let nowTime = performance.now();
		  if ((nowTime - firstTime) > 0.02 && touch.clientX != firstTouch.clientX) {
			let pt0 = get_xy(canvas, firstTouch);
			let pt1 = get_xy(canvas, lastTouch);
			let nx0 = pt0.x;
			let nx1 = pt1.x;
		        firstTouch = touch;
			firstTime = nowTime;
    			console.log("pan nx0= "+nx0+", nx1= "+nx1);
			let diff = maxx - minx;
			if (diff > 0) {
				zoom_to_new_xrange(minx - (nx1 - nx0), maxx - (nx1 - nx0), true);
			}
		  }
		  return;
		  let mouseEvent = new MouseEvent("mousemove", {
		    clientX: touch.clientX,
		    clientY: touch.clientY
		  });
		  canvas.dispatchEvent(mouseEvent);
		}, false);

		// Get the position of a touch relative to the canvas
		function getTouchPos(canvas, touchEvent) {
		  let rect = canvas.getBoundingClientRect();
		  return {
		    x: touchEvent.touches[0].clientX - rect.left,
		    y: touchEvent.touches[0].clientY - rect.top
		  };
		}
/*
		document.body.addEventListener("touchstart", function (e) {
		  if (e.target == canvas) {
		    //e.preventDefault();
		  }
		}, false);
		document.body.addEventListener("touchend", function (e) {
		  if (e.target == canvas) {
		    //e.preventDefault();
		  }
		}, false);
		document.body.addEventListener("touchmove", function (e) {
		  if (e.target == canvas) {
		    //e.preventDefault();
		  }
		}, false);
*/

	}
	my_touch_start(mycanvas);
	my_wheel_start2(mycanvas);

	function sortTot(a, b) {
		if (a[0] == b[0]) {
			let i = a[1], j = b[1];
			return chart_data.proc_arr[i].tid - chart_data.proc_arr[j].tid;
		}
		return b[0] - a[0];
	}


	let copy_canvas = false;
	if (mycanvas.width != (px_wide-2)) {
		mycanvas.width = px_wide - 2;
		mycanvas2.width = px_wide - 2;
		copy_canvas = true;
	}

	let proc_arr = [];
	let proc_tot = 0.0;
	for (let i=0; i < chart_data.proc_arr.length; i++) {
		//console.log("comm= "+chart_data.proc_arr[i].comm+" "+chart_data.proc_arr[i].pid+"/"+chart_data.proc_arr[i].tid+": "+ chart_data.proc_arr[i].total);
		proc_tot += chart_data.proc_arr[i].total;
		proc_arr.push([chart_data.proc_arr[i].total, i]);
	}
	proc_arr.sort(sortTot);
	//console.log("title= "+chart_data.title);
	/*
	for (let i=0; i < (proc_arr.length > 10 ? 10 : proc_arr.length); i++) {
		let j = proc_arr[i][1];
		console.log("proc_arr["+i+"].comm= "+chart_data.proc_arr[j].comm+", total= "+chart_data.proc_arr[j].total);
	}
	*/
	let tm_here_02 = performance.now();
	//console.log("proc_arr.len= "+ proc_arr.length);

	function xlate(tctx, xin, yin, uminx, umaxx, uminy, umaxy) {
		let xout = xPadding + Math.trunc((px_wide - xPadding) * (xin - uminx)/ (umaxx - uminx));
		let yout = Math.trunc((canvas_px_high(null) - yPadding) * (1.0 - (yin - uminy)/ (umaxy - uminy)));
		return [xout, yout];
	}

	let num_events = 0;
	let event_list = [];
	let event_lkup = {};
	let event_total = 0;
	let context_switch_event = "";
	let this_chart_prf_obj_idx = chart_data.prf_obj_idx;
	for (let j=0; j < chart_data.flnm_evt.length; j++) {
		if (chart_data.flnm_evt[j].event == "CSwitch") {
			context_switch_event = chart_data.flnm_evt[j].event;
		}
		if (chart_data.flnm_evt[j].event == "sched:sched_switch" ||
			chart_data.flnm_evt[j].event == "sched_switch") {
			context_switch_event = "sched_switch";
		}
	}
	if (ch_type == "line" || ch_type == "stacked") {
		let mx_cat = null, mnx, mxx, mny, mxy, mx_fe_idx=-1;
		let big_val = 1e20;
		mx_fe_idx = -1;
		if (chart_data.flnm_evt.length > 0) {
			mx_fe_idx = chart_data.flnm_evt[chart_data.flnm_evt.length-1].idx + 1;
		}
		// add a new subcat_rng for the __total__ line
		if (tot_line.evt_str.length > 0) {
			let already_added = false;
			num_events = chart_data.subcat_rng.length;
			for (let myi=0; myi < tot_line.evt_str.length; myi++) {
			already_added = false;
			for (let j=0; j < num_events; j++) {
				if (j == 0) {
					mx_cat = chart_data.subcat_rng[j].cat;
					mnx    = chart_data.subcat_rng[j].x0;
					mxx    = chart_data.subcat_rng[j].x1;
					mny    = chart_data.subcat_rng[j].y0;
					mxy    = chart_data.subcat_rng[j].y1;
					//mx_fe_idx = chart_data.subcat_rng[j].fe_idx;
				}
				if (mx_cat < chart_data.subcat_rng[j].cat) {
					mx_cat = chart_data.subcat_rng[j].cat;
				}
				if (mnx > chart_data.subcat_rng[j].x0) {
					mnx = chart_data.subcat_rng[j].x0;
				}
				if (mxx < chart_data.subcat_rng[j].x1) {
					mxx = chart_data.subcat_rng[j].x1;
				}
				if (mny > chart_data.subcat_rng[j].y0) {
					mny = chart_data.subcat_rng[j].y0;
				}
				if (mxy < chart_data.subcat_rng[j].y1) {
					mxy = chart_data.subcat_rng[j].y1;
				}
				if (chart_data.subcat_rng[j].event == tot_line.evt_str[myi]) {
					already_added = true;
					//tot_line.subcat_rng_idx = j;
					//tot_line.subcat_rng_idx[j] = true;
				}
			}
			if (!already_added) {
				let bef_len = chart_data.subcat_rng.length;
				let arr_len = tot_line.subcat_rng_arr.length;
				let use_fe_idx = mx_fe_idx + arr_len;
				let use_mx_cat = mx_cat + arr_len+1;
				chart_data.subcat_rng.push({x0:mnx, x1:mxx, y0:mny, y1:mxy, fe_idx:use_fe_idx, event:tot_line.evt_str[myi],
					 total:big_val, is_tot_line:true, cat:use_mx_cat, subcat:0, cat_text:tot_line.evt_str[myi]});
				if (chart_data.chart_tag == "CYCLES_PER_UOP_port_0_CHART") {
				console.log(sprintf("==tot_line: fe_idx= %d, mx_cat= %d, bef_len= %d", use_fe_idx, use_mx_cat, bef_len));
				}
				//tot_line.subcat_rng_idx = chart_data.subcat_rng.length-1;
				tot_line.subcat_rng_idx[chart_data.subcat_rng.length-1] = arr_len;
				tot_line.subcat_rng_arr.push(chart_data.subcat_rng.length-1);
			}
			}
		} else {
			//tot_line.subcat_rng_idx = -1;
			tot_line.subcat_rng_idx = {};
			tot_line.subcat_rng_arr = [];
		}

		num_events = chart_data.subcat_rng.length;
		for (let j=0; j < num_events; j++) {
			let fe_idx = chart_data.subcat_rng[j].fe_idx;
			event_list.push({event:chart_data.subcat_rng[j].cat_text, idx:chart_data.subcat_rng[j].cat,
				total:chart_data.subcat_rng[j].total, fe_idx:fe_idx, is_tot_line:chart_data.subcat_rng[j].is_tot_line});
			//if (tot_line.evt_str.length > 0 && (j+1) < num_events)
			if (typeof chart_data.subcat_rng[j].is_tot_line === 'undefined') {
				// don't add the fake __total__ total
				event_total += chart_data.subcat_rng[j].total;
			}
		}
		/*
		let str= tot_line.evt_str;
		event_list.push({event:str, idx:-1, total:0.0001, fe_idx:-1});
		*/
		event_list.sort(sortCat);
		function sortCat(a, b) {
			return b.total - a.total;
		}
		if (tot_line.evt_str.length > 0) {
			for (let j=0; j < num_events; j++) {
				if (typeof event_list[j].is_tot_line !== 'undefined') {
					event_list[j].total = 1.0e-6; // can't set it all the way to zero or else it will get dropped later
					tot_line.event_list_idx.push(j);
					if (chart_data.chart_tag == "CYCLES_PER_UOP_port_0_CHART") {
					console.log(sprintf("==evt_lst[%d], tl.eli.len= %d", j, tot_line.event_list_idx.length));
					}
					//console.log("zero out total for mx_cat= "+mx_cat+",j= "+j+",idx= "+event_list[j].idx);
					//break;
				}
			}
		}
	} else {
		num_events = chart_data.flnm_evt.length;
		for (let j=0; j < num_events; j++) {
			if (chart_data.flnm_evt[j].total == 0) {
				continue;
			}
			event_list.push(chart_data.flnm_evt[j]);
			event_total += chart_data.flnm_evt[j].total;
		}
		event_list.sort(sortEvt);
		function sortEvt(a, b) {
			return b.total - a.total;
		}
	}
	for (let j=0; j < event_list.length; j++) {
		let idx = event_list[j].idx;
		event_list[j].color = gcolor_def;
		event_lkup[idx] = j;
		for (let sci= 0; sci < tot_line.event_list_idx.length; sci++) {
		if (tot_line.event_list_idx[sci] == j) {
			tot_line.event_lkup_idx = idx;
		}
		}

		//console.log("evt= "+event_list[j].event+", tot= "+event_list[j].total+", lkup idx= "+idx+", j="+j);
	}
	let number_of_colors_proc = number_of_colors;
	let number_of_colors_events = number_of_colors;
	let proc_cumu = 0.0;
	let event_cumu = 0.0;
	let proc_rank = {};
	let legend_str = "";
	let event_select = {};
	let event_id_begin = 10000;
	//let c10 = d3.schemeCategory10;
	// cmd below is what the above cmd accomplishes but I don't have to include d3 anymore
	let c10 = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"];
	let c40 = gcolor_lst;
	let use_color_list = gcolor_lst;
	let legend_text_len = 0;
	let non_zero_legends = 0;
	let has_cpi  = false;
	let has_gips = false;
	let cpi_str = {};
	if (ch_type == "line" || ch_type == "stacked") {
		if (typeof event_list == 'undefined') {
			console.log("screw up here. event_list not defined. event_list=");
			console.log(event_list);
		}
	  if (event_list.length < c10.length) {
		use_color_list = c10;
		number_of_colors_events = c10.length;
	  } else {
		use_color_list = gcolor_lst;
		number_of_colors_events = gcolor_lst.length;
		//console.log("number of colors for events= "+number_of_colors_events);
	  }
	  for (let j=0; j < event_list.length; j++) {
		let i = event_list[j].idx; // this is fe_idx
		if ( typeof event_select[i] == 'undefined') {
			event_select[i] = [j, 'show'];
		}
		let this_tot = event_list[j].total;
		event_cumu += this_tot;
		let this_pct = 100.0* (this_tot/event_total);
		let pct_cumu = 100.0* (event_cumu/event_total);
		if (this_tot == 0) {
			continue;
		}
		let fe_idx = event_list[j].idx;
		if (typeof event_list[j].fe_idx != 'undefined') {
			// so we are doing a non-event list (like a chart by process)
			fe_idx = event_list[j].fe_idx;
		}
		let nm = event_list[j].event;
		let flnm = event_list[j].filename_text;
		if (typeof flnm == 'undefined') {
			//console.log("fe_idx= "+fe_idx+", flnm_evt.sz= "+ chart_data.flnm_evt.length+", ev= "+nm);
			if (typeof fe_idx != 'undefined' && fe_idx > -1) {
				let fentr = chart_data.flnm_evt[fe_idx];
				let fbin = '';
				let ftxt = '';
				let fevt = '';
			   	if (typeof fentr != 'undefined') {
					fbin = fentr.filename_bin;
					ftxt = fentr.filename_text;
					fevt = fentr.event;
				} else {
					//console.log("__null fbin for evt= "+nm);
				}
				flnm = "bin:"+fbin + ", txt:"+ftxt + ", event:"+fevt;
			}
		}
		let title = "event["+j+"]= "+nm+" samples: "+this_tot+", %tot_samples= "+this_pct.toFixed(4)+"%, cumu_pct: "+pct_cumu.toFixed(4)+"%, file="+flnm;
		let disp_str;
		non_zero_legends++;
		if (non_zero_legends < 20) {
			disp_str = hvr_clr+"_legend_top_20";
		} else {
			disp_str = hvr_clr+"_legend_top_20_plus";
		}
		if (j < number_of_colors_events) {
			event_list[j].color = use_color_list[j];
		}
		let clr = event_list[j].color;
		event_list[j].legend_num = event_id_begin+j;
		let leg_num = event_list[j].legend_num;
		let evt_str = "";
		if (ch_type == "line") {
			//evt_str = chart_data.y_by_var+" ";
		}
		legend_text_len += evt_str.length + nm.length;

		legend_str += '<span  title="'+title+'" class="'+disp_str+'" style="margin-right:5px; white-space: nowrap; display: inline-block;"><span id="'+hvr_clr+'_legendp_'+leg_num+'" class="legend_square" style="background-color: '+clr+'; display: inline-block"></span><span id="'+hvr_clr+'_legendt_'+leg_num+'">'+evt_str+nm+'</span></span>';
	  }
	} else {
		let cpi_tm = "";
		let cpi_tm2 = "";
		let cpi_cycles = "";
		let cpi_cycles2 = "";
		for (let j=0; j < event_list.length; j++) {
			let nm = event_list[j].event;
			//if (ev != "CSwitch" && ev != "sched:sched_switch" && ev != "cpu-clock") {
			if (!event_list[j].has_callstacks) {
				continue;
			}
			if (nm == "CSwitch" || nm == "sched:sched_switch") {
				cpi_tm2 = nm;
			}
			if (nm == "cpu-clock") {
				cpi_tm = nm;
			}
			if (nm == "cycles") {
				cpi_cycles = nm;
			}
			if (nm == "ref-cycles") {
				cpi_cycles2 = nm;
			}
		}
		if (cpi_tm == "" && cpi_tm2 != "") {
			cpi_tm = cpi_tm2;
		}
		if (cpi_cycles == "" && cpi_cycles2 != "") {
			cpi_cycles = cpi_cycles2;
		}
	  for (let j=0; j < event_list.length; j++) {
		let i = event_list[j].idx;
		let nm = event_list[j].event;
		if ( typeof event_select[i] == 'undefined') {
			event_select[i] = [j, 'show'];
		}
		let this_tot = event_list[j].total;
		event_cumu += this_tot;
		if (this_tot == 0) {
			continue;
		}
		let fe_idx = event_list[j].idx;
		if (fe_idx > -1 && chart_data.flnm_evt[fe_idx].prf_obj_idx != this_chart_prf_obj_idx) {
			continue;
		}
		if (cpi_cycles != "" && nm == cpi_cycles) {
			cpi_str.cycles = nm;
		} else if (nm == "instructions") {
			cpi_str.instructions = nm;
		} else if (cpi_tm != "" && nm == cpi_tm) {
			cpi_str.time = nm;
		}
		non_zero_legends++;
		let this_pct = 100.0* (this_tot/event_total);
		let pct_cumu = 100.0* (event_cumu/event_total);
		let flnm = event_list[j].filename_text;
		if (typeof flnm == 'undefined') {
			if (typeof fe_idx != 'undefined') {
				flnm = "bin:"+chart_data.flnm_evt[fe_idx].filename_bin + ", txt:"+chart_data.flnm_evt[fe_idx].filename_text;
			}
		}
		let title = "event["+j+"]= "+nm+" samples: "+this_tot+", %tot_samples= "+this_pct.toFixed(4)+"%, cumu_pct: "+pct_cumu.toFixed(4)+"%, file="+flnm;
		let disp_str;
		if (non_zero_legends < 20) {
			disp_str = hvr_clr+"_legend_top_20";
		} else {
			disp_str = hvr_clr+"_legend_top_20_plus";
		}
		if (j < number_of_colors_events) {
			event_list[j].color = gcolor_lst[j];
		}
		let clr = event_list[j].color;
		event_list[j].legend_num = event_id_begin+j;
		let leg_num = event_list[j].legend_num;
		legend_text_len += 5 + nm.length;

		legend_str += '<span  title="'+title+'" class="'+disp_str+'" style="margin-right:5px; white-space: nowrap; display: inline-block;"><span id="'+hvr_clr+'_legendp_'+leg_num+'" class="legend_square" style="background-color: '+clr+'; display: inline-block"></span><span id="'+hvr_clr+'_legendt_'+leg_num+'">evt: '+nm+'</span></span>';
	  }
	  if (typeof cpi_str.cycles != 'undefined' &&
			  typeof cpi_str.instructions != 'undefined' &&
			  typeof cpi_str.time != 'undefined') {
		  has_cpi = true;
	  }
	  if ( typeof cpi_str.instructions != 'undefined' &&
			  typeof cpi_str.time != 'undefined') {
		  has_gips = true;
	  }
	  for (let j=0; j < proc_arr.length; j++) {
		let i = proc_arr[j][1];
		proc_rank[i] = j;
		if ( typeof proc_select[i] == 'undefined') {
			proc_select[i] = [j, 'show'];
		}
		let this_tot = chart_data.proc_arr[i].total;
		proc_cumu += this_tot;
		let this_pct = 100.0* (this_tot/proc_tot);
		let pct_cumu = 100.0* (proc_cumu/proc_tot);
		let nm = chart_data.proc_arr[i].comm+" "+chart_data.proc_arr[i].pid+"/"+chart_data.proc_arr[i].tid;
		let title = "comm["+j+"],"+i+"= "+nm+": tot_secs: "+ chart_data.proc_arr[i].total+", %tot_secs= "+this_pct.toFixed(4)+"%, cumu_pct: "+pct_cumu.toFixed(4)+"%";
		let disp_str;
		if (j < 20) {
			disp_str = hvr_clr+"_legend_top_20";
		} else {
			disp_str = hvr_clr+"_legend_top_20_plus";
		}
		//console.log(title);
		let clr;
		if (j < number_of_colors_proc) {
			clr = gcolor_lst[j];
		} else {
			clr = gcolor_def; // lightgrey for events above 'number of colors' rank
		}
		legend_text_len += nm.length;
		legend_str += '<span  title="'+title+'" class="'+disp_str+'" style="margin-right:5px; white-space: nowrap; display: inline-block;"><span id="'+hvr_clr+'_legendp_'+j+'" class="legend_square" style="background-color: '+clr+'; display: inline-block"></span><span id="'+hvr_clr+'_legendt_'+j+'">'+nm+'</span></span>';
	  }
	}
	//console.log("legend_str="+legend_str);
	mylegend.innerHTML = legend_str;
	mylegend.setAttribute("style", "display: inline-block; max-height:200px;word-wrap: break-word;overflow: auto;");
	showLegend(hvr_clr, 'hide_all');
	if (legend_text_len < 100) {
		showLegend(hvr_clr, 'show_top_20');
	}
	//console.log("legend text_len= "+legend_text_len);
	//showLegend(hvr_clr, 'hide_all');
	function legend_add_listener(id_nm, rank, proc_idx, typ_legend) {
		let ele = document.getElementById(id_nm);
		if (ele != null) {
		ele.addEventListener('click', legend_click, false);
		ele.addEventListener('dblclick', legend_dblclick, false);
		ele.addEventListener('mouseenter', legend_mouse_enter, false);
		ele.addEventListener('mouseout', legend_mouse_out, false);
		ele.rank = rank;
		ele.proc_idx = proc_idx;
		ele.typ_legend = typ_legend;
		}
	}
	if (ch_type != "line" && ch_type != "stacked") {
		for (let j=0; j < proc_arr.length; j++) {
			let i = proc_arr[j][1];
			let id_nm = hvr_clr+'_legendp_'+j;
			legend_add_listener(id_nm, j, i, 'proc');
			id_nm = hvr_clr+'_legendt_'+j;
			legend_add_listener(id_nm, j, i, 'proc');
		}
	}
	for (let j=0; j < event_list.length; j++) {
		let fe_idx = event_list[j].idx;
		let leg_num = event_list[j].legend_num;
		let id_nm = hvr_clr+'_legendp_'+leg_num;
		if (j > non_zero_legends) {
			//console.log("leg.skp ev= "+event_list[j].event);
			continue;
		}
		legend_add_listener(id_nm, j, fe_idx, 'event');
		id_nm = hvr_clr+'_legendt_'+leg_num;
		legend_add_listener(id_nm, j, fe_idx, 'event');
	}
	function legend_click(evt)
	{
		//console.log(evt);
		//console.log(evt.target.parentNode.rank);
		let ele_nm = evt.target.id;
		let ele = document.getElementById(ele_nm);
		let i = ele.rank;
		let proc_idx = ele.proc_idx;
		let typ = ele.typ_legend;
		let cur_state;
		if (typ == 'proc') {
			cur_state = proc_select[proc_idx][1];
		} else {
			cur_state = event_select[proc_idx][1];
		}
		if (cur_state != 'show' && cur_state != 'hide') {
			cur_state = 'show';
		}
		let prev_state = cur_state;
		if (cur_state == 'show') {
			cur_state = 'hide';
			ele.style.opacity = '0.5';
		} else {
			cur_state = 'show';
			ele.style.opacity = '1.0';
		}
		//proc_select[proc_idx] = [i, cur_state, prev_state];
		if (typ == 'proc') {
			proc_select[proc_idx] = [i, cur_state];
		} else {
			event_select[proc_idx] = [i, cur_state];
		}
		console.log("legend exit id= "+ele_nm+", rank= "+i+", proc_idx= "+proc_idx+", cur_state= "+cur_state);
		chart_redraw("lgnd_clk");
	}
	function legend_dblclick(evt)
	{
		//console.log(evt);
		//console.log(evt.target.parentNode.rank);
		let ele_nm = evt.target.id;
		let ele = document.getElementById(ele_nm);
		let rnk = ele.rank;
		let proc_idx = ele.proc_idx;
		let typ = ele.typ_legend;
		let cur_state;
		if (typ == 'proc') {
			cur_state = proc_select[proc_idx][1];
		} else {
			cur_state = event_select[proc_idx][1];
		}
		console.log("dbl clicked id= "+evt.target.parentNode.id+", rank= "+rnk+", proc_idx= "+proc_idx+", cur_state= "+cur_state);
		if (cur_state != 'show' && cur_state != 'hide') {
			cur_state = 'show';
		}
		let tgl_state = cur_state;
		let tgl_opac;
		// the single click rtn gets called first, undo it now.

		if (cur_state == 'show') {
			tgl_opac = '0.5';
			tgl_state = 'hide';
			ele.style.opacity = '1.0';
		} else {
			tgl_opac = '1.0';
			tgl_state = 'show';
			ele.style.opacity = '0.5';
		}
/*
*/
		if (typ == 'proc') {
			proc_select[proc_idx] = [rnk, cur_state];
			for (let j=0; j < proc_arr.length; j++) {
				let i = proc_arr[j][1];
				if (i != proc_idx) {
					proc_select[i] = [j, tgl_state];
					let mele_nm = hvr_clr+'_legendt_'+j;
					let mele = document.getElementById(mele_nm);
					mele.style.opacity = tgl_opac;
				}
			}
			proc_select[proc_idx] = [rnk, cur_state];
			//console.log("proc_select:");
			//console.log(proc_select);
		} else {
			event_select[proc_idx] = [rnk, cur_state];
			for (let j=0; j < event_list.length; j++) {
				let i = event_list[j].idx;
				if (i != proc_idx) {
					event_select[i] = [j, tgl_state];
					let leg_num = event_list[j].legend_num;
					let mele_nm = hvr_clr+'_legendt_'+leg_num;
					let mele = document.getElementById(mele_nm);
					if (mele != null) {
						mele.style.opacity = tgl_opac;
					}
				}
			}
			console.log("dblclick event_select:");
			event_select[proc_idx] = [rnk, cur_state];
		}
		console.log("legend dbl click 2nd_draw id= "+ele_nm+", rank= "+rnk+", proc_idx= "+proc_idx+", cur_state= "+cur_state);
		chart_redraw("lgnd_dblclk");
	}
	let last_legend_mouse_evt = null;
	let last_legend_timeout = null;
	function legend_hover(evt)
	{
		clearTimeout(last_legend_timeout);
		last_legend_timeout = null;
		let ele_nm = evt.target.id;

		let ele = document.getElementById(ele_nm);
		let i = ele.rank;
		let typ = ele.typ_legend;
		let proc_idx = ele.proc_idx;
		//console.log("legend enter id= "+ele_nm+", rank= "+i+", proc_idx= "+proc_idx+", typ= "+typ);
		if (typ == 'proc') {
			let prev_state = proc_select[proc_idx][1];
			proc_select[proc_idx] = [i, 'highlight'];
			chart_redraw("lgnd_hvr_hghlght_proc");
			proc_select[proc_idx] = [i, prev_state];
		} else {
			let prev_state = event_select[proc_idx][1];
			event_select[proc_idx] = [i, 'highlight'];
			chart_redraw("lgnd_hvr_hghlght_evnt");
			event_select[proc_idx] = [i, prev_state];
		}

		if (last_legend_timeout != null) {
			clearTimeout(last_legend_timeout);
			last_legend_timeout = null;
		}
	}
	function legend_mouse_enter(evt)
	{
		//console.log(evt);
		//console.log(evt.target.parentNode);
		//console.log(evt.target.parentNode.rank);
		//let ele_nm = evt.target.parentNode.id;
		let ele_nm = evt.target.id;
		let ele = document.getElementById(ele_nm);
		let i = ele.rank;
		let typ = ele.typ_legend;
		let proc_idx = ele.proc_idx;
		if (1==1 && typ == 'event') {
			console.log("__legend_mouse_enter id= "+evt.target.parentNode.id+", rank= "+i+", proc_idx= "+
				proc_idx+", typ= "+typ+",nm="+event_list[i].event);
		}
		if (1==20) {
			console.log("__legend_mouse_enter ele= "+ele_nm);
		}
		// this is a delay so we don't redraw a bunch when all we are doing is passing over the legend entries.
		last_legend_timeout = setTimeout(legend_hover, 200, evt);
		return;

	}
	function legend_mouse_out(evt)
	{
		//console.log(evt);
		//console.log(evt.target.parentNode);
		//console.log(evt.target.parentNode.rank);
		//let ele_nm = evt.target.parentNode.id;
		if (last_legend_timeout != null) {
			clearTimeout(last_legend_timeout);
			last_legend_timeout = null;
			return;
		}
		let ele_nm = evt.target.id;
		let ele = document.getElementById(ele_nm);
		let i = ele.rank;
		let proc_idx = ele.proc_idx;
		let typ = ele.typ_legend;
		let cur_state;
		if (typ == 'proc') {
			cur_state = proc_select[proc_idx][1];
			proc_select[proc_idx] = [i, cur_state];
		} else {
			cur_state = event_select[proc_idx][1];
			event_select[proc_idx] = [i, cur_state];
		}
		if (1==20) {
			console.log("legend_mouse_out: cur_state= "+cur_state);
			console.log("legend exit id= "+ele_nm+", rank= "+i+", proc_idx= "+proc_idx+", typ= "+typ);
		}
		chart_redraw("lgnd_mouseout");
	}
	let subcats = [];
	let subcat_cs_2_sidx_hash = {}; // indexed by [cat][subcat], returns subcat_idx
	let subcat_sidx_2_cs_hash = {}; // indexed by [subcat_idx], returns [cat, subcat]
	let ylkup = [];

	function redo_ylkup(ctx)
	{
		subcats = [];
		subcat_cs_2_sidx_hash = {}; // indexed by [cat][subcat], returns subcat_idx
		subcat_sidx_2_cs_hash = {}; // indexed by [subcat_idx], returns [cat, subcat]
		ylkup = [];
		//console.log("chart_data.subcat_rng.length= " + chart_data.subcat_rng.length);
		for (let i=0; i < chart_data.subcat_rng.length; i++) {
			let cat = chart_data.subcat_rng[i].cat;
			subcat_cs_2_sidx_hash[cat] = {};
		}
		for (let i=0; i < chart_data.subcat_rng.length; i++) {
			let beg = xlate(ctx, chart_data.subcat_rng[i].x0, chart_data.subcat_rng[i].y0, minx, maxx, miny, maxy);
			let end = xlate(ctx, chart_data.subcat_rng[i].x1, chart_data.subcat_rng[i].y1, minx, maxx, miny, maxy);
			subcats.push(chart_data.subcat_rng[i]);
			let cat = chart_data.subcat_rng[i].cat;
			let subcat = chart_data.subcat_rng[i].subcat;
			subcat_cs_2_sidx_hash[cat][subcat] = i;
			let subcat_idx = subcat_cs_2_sidx_hash[cat][subcat];
			//ylkup[subcat_idx] = [end[1], end[0], beg[1], beg[0], i];
			//ylkup.push([end[1], end[0], beg[1], beg[0], i, chart_data.subcat_rng[i].event]);
			ylkup.push([end[1], 0, beg[1], 0, i, chart_data.subcat_rng[i].event]);
			subcat_sidx_2_cs_hash[i] = [cat, subcat];
		}
		ylkup.sort(sortFunction);
	}


	// ctx work begin
	let tm_here_03 = performance.now();
	let x_txt_px_max = 0;
	let font_sz = 14;
	let ctx = mycanvas.getContext("2d");

	redo_ylkup(ctx);

	ctx.font = font_sz + 'px Arial';
	let y_axis_decimals = 4;
	if (ch_type == "line" || ch_type == "stacked") {
		let str;
		if (chart_data.y_fmt != "") {
			str = vsprintf(chart_data.y_fmt, [maxy]);
		} else {
			str = maxy.toFixed(y_axis_decimals);
		}
		let sz = ctx.measureText(str).width;
		if (x_txt_px_max < sz) {
			x_txt_px_max = sz;
		}
	} else {
		for (let i=0; i < chart_data.subcat_rng.length; i++) {
			if (chart_data.subcat_rng[i].subcat != 0) {
				continue;
			}
			let str = chart_data.y_by_var + " " + chart_data.subcat_rng[i].cat_text;
			let sz = ctx.measureText(str).width;
			if (x_txt_px_max < sz) {
				x_txt_px_max = sz;
			}
		}
	}
	xPadding = Math.trunc(font_sz + x_txt_px_max + 10);

	let tm_here_04 = performance.now();
	let lkup = [];
	let lkup_use_linearSearch = [];
	let flm_obj = {};
	let fl_end_sum = 0;
	let fl_end_sum1 = 0;
	let fl_end_sum2 = 0;
	let fl_id = -1;
	let current_tooltip_text = "";
	let current_tooltip_shape = -1;

	let dbg_cntr2 = 0;

	function fl_add_val(evt, component_evt, val, obj)
	{
		if (has_gips && evt == "GIPS") {
			if (component_evt == cpi_str.time) {
				obj.sum0 += val;
			} else if (component_evt == cpi_str.instructions) {
				obj.sum2 += val;
			}
		} else if (has_cpi && evt == "CPI") {
			if (component_evt == cpi_str.time) {
				obj.sum0 += val;
			} else if (component_evt == cpi_str.cycles) {
				obj.sum1 += val;
			} else { // assume instructions
				obj.sum2 += val;
			}
		}
		else
		{
			obj.sum0 += val;
		}
	}

	function build_flame(evt, cs_unit, callstack, val, txt_fld, cs_color, component_evt) {
		// struct is:
		//  holder (typ:0) obj has a list of all the siblings (sections of flame at same level with common parent)
		/*     lvl 1       create a holder for level 1
		*       /  +===\
		*      0       1     so lvl 1's holder has 2 sibling objs
		*    lvl 2   lvl 2   sib obj 0 and 1 have kids so create a holder for the kids of sib obj 0 and a holder for the kids of sib obj 1
		*    / \    /  |  \
		*   2   3   4  5   6    there are be 2 sib objs in the first lvl 2 holder and 3 sib objs in the 2nd lvl2 holder
		*   the 'dad' pointer in a sib points to its holder.
		*   the 'dad' pointer in a holder points to the sib obj who created the holder obj
		*   We should be able to walk back from the tip of the flame to base by following the dad pointers.
        */
		if (!(evt in g_fl_hsh[file_tag_idx])) {
			g_fl_hsh[file_tag_idx][evt] = g_fl_arr[file_tag_idx].length;
			g_fl_arr[file_tag_idx].push({event: evt, unit:cs_unit, level0_tot:-1.0, component_evt:component_evt});
			let ln = g_fl_arr[file_tag_idx].length-1;
			if (lhs_menu_ch_list_state == 0) {
				for (let kk=0; kk < lhs_menu_ch_list.length; kk++) {
					if (txt_fld == lhs_menu_ch_list[kk].hvr_nm) {
						let ins = kk+g_fl_arr[file_tag_idx].length;
						//let o = lhs_menu_ch_list[kk];
						let o = new Object({});
						o = Object.assign({}, lhs_menu_ch_list[kk]);
						//let mycanvas3_nm_title = 'canvas3'+g_fl_hsh[file_tag_idx][evt]+'_'+txt_fld+"_title";
						let fl_title_str = "Flamegraph for event: "+evt;
						let mycanvas3_nm_all = 'canvas3'+g_fl_hsh[file_tag_idx][evt]+'_'+txt_fld+"_all";
						o.hvr_nm = mycanvas3_nm_all;
						o.fl_hsh[evt] = g_fl_hsh[file_tag_idx][evt];
						o.fl_arr.push(g_fl_arr[file_tag_idx][g_fl_arr[file_tag_idx].length-1]);
						//o.fl_arr.push(g_fl_arr[file_tag_idx][g_fl_hsh[file_tag_idx]]);
						o.title = fl_title_str;
						lhs_menu_ch_list.splice(ins, 0, o);
						//console.log("got match on fl txtfld= "+txt_fld+", evt= "+evt+", g_fl_hsh[file_tag_idx][evt]= "+g_fl_hsh[file_tag_idx][evt]);
						break;
					}
				}
			}
		}
		let cs_idx = -1;
		for (let kk=0; kk < lhs_menu_ch_list.length; kk++) {
			if (txt_fld == lhs_menu_ch_list[kk].hvr_nm) {
				cs_idx = lhs_menu_ch_list[kk].fl_hsh[evt];
			}
		}
		if (!(cs_idx in g_fl_obj[file_tag_idx])) {
			g_fl_obj_rt[file_tag_idx][cs_idx] = null;
			g_fl_obj[file_tag_idx][cs_idx] = null;
		}
		if (cs_idx == -1) {
			console.log("ummm... cs_idx= -1 ... missed looking up evt");
			return;
		}
		let csz = callstack.length;
		if (g_fl_obj_rt[file_tag_idx][cs_idx] == null && g_fl_obj[file_tag_idx][cs_idx] == null) {
			fl_id++;
			g_fl_obj[file_tag_idx][cs_idx] = new Object({sum:0, sum1:0, sum2:0, lvl:0, lvl_sum:0, lvl_sum1:0, lvl_sum2:0, typ:0, key:callstack[0], sib_hsh:{}, sib_arr:[], dad:null, kids:null, id:fl_id, beg:0, end:0, color:cs_color, krnl:0});
			g_fl_obj_rt[file_tag_idx][cs_idx] = g_fl_obj[file_tag_idx][cs_idx];
			fl_end_sum = 0;
			fl_end_sum1 = 0;
			fl_end_sum2 = 0;
			//console.log("flm: added nm= "+nm+", lvl= "+i);
		}
		g_fl_obj[file_tag_idx][cs_idx] = g_fl_obj_rt[file_tag_idx][cs_idx];
		for (let i=0; i < csz; i++) {
			let nm = callstack[i];
			let idx = -1;
			if (!(nm in g_fl_obj[file_tag_idx][cs_idx].sib_hsh)) {
				idx = g_fl_obj[file_tag_idx][cs_idx].sib_arr.length;
				g_fl_obj[file_tag_idx][cs_idx].sib_hsh[nm] = idx;
				// new flame obj dad points to the holder obj
				fl_id++;
				let krnl = 0;
				if (nm.includes('[krnl]')) {
					krnl = 1;
				}
				g_fl_obj[file_tag_idx][cs_idx].sib_arr.push(new Object({sum:0, sum1:0, sum2:0, lvl:i, lvl_sum:0, lvl_sum1:0, lvl_sum2:0, typ:1, key:nm, sib_hsh:{}, sib_arr:[], dad:g_fl_obj[file_tag_idx][cs_idx], kids:null, id:fl_id, beg:0, end:0, color:cs_color, krnl:krnl}));
				//console.log("flm: new sib_hsh["+nm+"]="+idx);
			}
			{
				let op = {sum0:g_fl_obj[file_tag_idx][cs_idx].lvl_sum,
					sum1:g_fl_obj[file_tag_idx][cs_idx].lvl_sum1,
					sum2:g_fl_obj[file_tag_idx][cs_idx].lvl_sum2};
				fl_add_val(evt, component_evt, val, op);
				g_fl_obj[file_tag_idx][cs_idx].lvl_sum = op.sum0;
				g_fl_obj[file_tag_idx][cs_idx].lvl_sum1 = op.sum1;
				g_fl_obj[file_tag_idx][cs_idx].lvl_sum2 = op.sum2;
			}
			{
				let op = {sum0:g_fl_obj[file_tag_idx][cs_idx].sum,
					sum1:g_fl_obj[file_tag_idx][cs_idx].sum1,
					sum2:g_fl_obj[file_tag_idx][cs_idx].sum2};
				fl_add_val(evt, component_evt, val, op);
				g_fl_obj[file_tag_idx][cs_idx].sum = op.sum0;
				g_fl_obj[file_tag_idx][cs_idx].sum1 = op.sum1;
				g_fl_obj[file_tag_idx][cs_idx].sum2 = op.sum2;
			}
			idx = g_fl_obj[file_tag_idx][cs_idx].sib_hsh[nm];
			if ((i+1) == csz) {
				let op = {sum0:g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].sum,
					sum1:g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].sum1,
					sum2:g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].sum2};
				fl_add_val(evt, component_evt, val, op);
				g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].sum = op.sum0;
				g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].sum1 = op.sum1;
				g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].sum2 = op.sum2;
				/*g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].sum += val;*/
			}
			if ((i+1) < csz && g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].kids == null) {
				// this flame goes above current level and doesn't already have holder for next level
				// create the holder now and set the dad and kids pointers
				// new holder obj dad points to this flame branch.
				fl_id++;
				let g_fl_obj2 = new Object({sum:0, sum1:0, sum2:0, lvl:i+1, lvl_sum:0, lvl_sum1:0, lvl_sum2:0, typ:0, key:callstack[i+1], sib_hsh:{}, sib_arr:[], dad:g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx], kids:null, id:fl_id, beg:0, end:0, color:cs_color, krnl:0});
				g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].kids = g_fl_obj2;
			}
			if ((i+1) == csz) {
				let op = {sum0:fl_end_sum, sum1:fl_end_sum1, sum2:fl_end_sum2};
				fl_add_val(evt, component_evt, val, op);
				fl_end_sum  = op.sum0;
				fl_end_sum1 = op.sum1;
				fl_end_sum2 = op.sum2;
			}
			{
				let op = {sum0:g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].lvl_sum,
					sum1:g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].lvl_sum1,
					sum2:g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].lvl_sum2};
				fl_add_val(evt, component_evt, val, op);
				g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].lvl_sum = op.sum0;
				g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].lvl_sum1 = op.sum1;
				g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].lvl_sum2 = op.sum2;
			}
			g_fl_obj[file_tag_idx][cs_idx] = g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].kids;
		}
		did_prt++;
	}

	let dbg_cntr3 = 0;
	function build_flame_rpt(txt_fld, input_cs_evt) {
		let tsum = 0;
		let use_fl_arr = [];
		let use_fl_hsh = {};
		let cs_idx = -1;
		canvas3_px_high = mycanvas.height;
		if (input_cs_evt != "") {
			//console.log("b_fl_rpt: "+input_cs_evt+",txt_fld= "+txt_fld);
			let got_it = false;
			for (let kk=0; kk < lhs_menu_ch_list.length; kk++) {
				for (let ii=0; ii < lhs_menu_ch_list[kk].fl_arr.length; ii++) {
					if (input_cs_evt != "" && lhs_menu_ch_list[kk].fl_arr[ii].event == input_cs_evt) {
						//use_fl_hsh = lhs_menu_ch_list[kk].fl_hsh;
						//use_fl_arr = lhs_menu_ch_list[kk].fl_arr;
						use_fl_hsh = g_fl_hsh[file_tag_idx];
						use_fl_arr = g_fl_arr[file_tag_idx];
						got_it = true;
						break;
					}
				}
				if (got_it) {
					break;
				}
			}
		} else {
			for (let kk=0; kk < lhs_menu_ch_list.length; kk++) {
				if (txt_fld == lhs_menu_ch_list[kk].hvr_nm) {
					use_fl_hsh = lhs_menu_ch_list[kk].fl_hsh;
					use_fl_arr = lhs_menu_ch_list[kk].fl_arr;
					break;
				}
			}
		}
		/*
		*/
		for (let cs_idx=0; cs_idx < use_fl_arr.length; cs_idx++) {
		//console.log("build_flame_rpt: cs_idx="+cs_idx+",g_fl_arr.evt= "+g_fl_arr[file_tag_idx][cs_idx].event+", use_arr.evt="+use_fl_arr[cs_idx].event);
		//let fl_menu_ch_title_str = "Flamegraph for event: "+g_fl_arr[file_tag_idx][cs_idx].event;
		if (input_cs_evt == context_switch_event+"_offcpu" && ++dbg_cntr3 < 10) {
			for (let cs_idx2=0; cs_idx2 < use_fl_arr.length; cs_idx2++) {
				//console.log("g_f_a["+cs_idx2+"]="+use_fl_arr[cs_idx2].event);
			}
			//console.log("b_fl_rpt: "+input_cs_evt+",cs_idx= "+cs_idx+", g_fl_arr= "+ g_fl_arr[file_tag_idx][cs_idx].event);
		}
		if (input_cs_evt != "" && use_fl_arr[cs_idx].event != input_cs_evt) {
			continue;
		}
		//console.log("bld_fl_rpt evt= "+use_fl_arr[cs_idx].event+", inp_evt= "+input_cs_evt);
		if (input_cs_evt == context_switch_event+"_offcpu" && ++dbg_cntr3 < 10) {
			//console.log("b_fl_rpt: "+input_cs_evt);
		}
		g_fl_obj[file_tag_idx][cs_idx] = g_fl_obj_rt[file_tag_idx][cs_idx];

		function fl_xlate_ctx(ele, xin, yin, uminx, umaxx, uminy, umaxy) {
			//let xPadding = 2;
			//let yPadding = 2;
			let xPadding = 0;
			let yPadding = 2.0;
			//let xout =  Math.trunc((px_wide - xPadding) * (xin - uminx)/ (umaxx - uminx));
			//let yout = Math.trunc((px_high - yPadding) * (1.0 - (yin - uminy)/ (umaxy - uminy)));
			let xout = ((px_wide - xPadding) * (xin - uminx)/ (umaxx - uminx));
			let yout = ((canvas_px_high(ele) - yPadding) * (1.0 - (yin - uminy)/ (umaxy - uminy)));
			return [xout, yout];
		}
		let mycanvas3_nm_all = 'canvas3'+cs_idx+'_'+txt_fld+"_all";
		let myck = document.getElementById(mycanvas3_nm_all);
		if (myck != null) {
			while (myck.firstChild) {
    				myck.removeChild(myck.firstChild);
			}
			myck.parentNode.removeChild(myck);
		}
		let mycanvas3_nm     = 'canvas3'+cs_idx+'_'+txt_fld;
		let mycanvas3_nm_txt = 'canvas3'+cs_idx+'_'+txt_fld+"_txt";
		let mycanvas3_nm_title = 'canvas3'+cs_idx+'_'+txt_fld+"_title";
		let mycanvas3_nm_tt  = 'tooltip3'+cs_idx+'_'+txt_fld;
		let mycanvas3 = document.getElementById(mycanvas3_nm);
		if (mycanvas3 != null) {
			mycanvas3.parentNode.removeChild(mycanvas3);
			mycanvas3 = document.getElementById(mycanvas3_nm);
		}
		if (mycanvas3 == null) {
			//addElement ('canvas', mycanvas3_nm, 'chart_anchor');
			let str='<div class="center-outer-div"><div class="center-inner-div" id="'+mycanvas3_nm_title+'"></div></div><div class="tooltip"><canvas id="'+mycanvas3_nm+'" width="'+(px_wide-2)+'" height="'+(canvas3_px_high-2)+'" style="border:1px solid #000000;"></canvas><span id="'+mycanvas3_nm_txt+'"></span><span class="tooltiptext" id="'+mycanvas3_nm_tt+'"></span></div><hr>';
			//addElement ('div', mycanvas3_nm_all, 'chart_anchor', 'before');
			addElement ('div', mycanvas3_nm_all, hvr_clr+'_bottom', 'before');
			//addElement ('div', mycanvas3_nm_all, use_div, 'after');
			//addElement ('div', mycanvas3_nm_all, 'chart_anchor', 'after');
			//addElement ('div', mycanvas3_nm_all, hvr_clr, 'after');
			let ele = document.getElementById(mycanvas3_nm_all);
			ele.innerHTML = str;
		}
		//console.log("cs_idx= "+cs_idx+", mycan3 nm= "+mycanvas3_nm);
		mycanvas3         = document.getElementById(mycanvas3_nm);
		let mycanvas3_txt = document.getElementById(mycanvas3_nm_txt);
		let mytooltip3    = document.getElementById(mycanvas3_nm_tt);
		let mytitle3      = document.getElementById(mycanvas3_nm_title);
		let ctx3 = mycanvas3.getContext("2d");
		ctx3.font = font_sz + 'px Arial';
		if (cs_idx > 0) {
			//return 1;
		}
		//console.log("set sldr_cur= "+sldr_cur);
		//myhvr_clr = document.getElementById(hvr_clr);
		let ostr = "";
		let sum = 0;
		let sum0 = 0;
		let fl_lkup = []
		let tm_cutoff = 0.005;
		let fl_hsh = {};
		let lvl0_sum = 0.0;
		function fl_xlate_x(tot_wide, x) {
			return x/tot_wide;
		}
		function build_fl_rpt(cs_idx, lvl_arr, lvl_in, cs_str) {
			let fl_obj = lvl_arr[lvl_in][1];
			let lvl = lvl_arr[lvl_in][0];
			if (fl_lkup.length <= lvl) {
				fl_lkup.push([]);
				//console.log("push fl_lkup lvl= "+lvl);
				if (fl_lkup.length <= fl_obj.lvl) {
					console.log("screw up here. fl_lkup.length= "+fl_lkup.length+", lvl= "+fl_obj.lvl);
				}
			}
			if (fl_obj.lvl == 0) {
				//console.log("sum0 lvl"+lvl+" i= "+i+", sum0= "+(1.0e-9*sum0)+", nm= "+fl_obj.sib_arr[i].key+",kids="+(fl_obj.sib_arr[i].kids == null));
				//sum0 += fl_obj.sib_arr[i].sum;
				sum0 += fl_obj.sum;
				lvl0_sum = fl_obj.lvl_sum;
			}
			if (fl_obj.id in fl_hsh) {
				console.log("dup fl_obj lvl= "+fl_obj.lvl+", len= "+fl_lkup[fl_obj.lvl].length+" appears= "+(fl_hsh[fl_obj.id]+1)+" times");
			} else {
				fl_hsh[fl_obj.id] = 0;
			}

			fl_hsh[fl_obj.id]++;
			fl_lkup[fl_obj.lvl].push({fl_obj:fl_obj});
			let lvl_cumu = 0.0;
			let lvl_tot = fl_obj.lvl_sum;
			for (let i=0; i < fl_obj.sib_arr.length; i++) {
				if (i == 0) {
					lvl_cumu = 0.0;
				}
				if (1==1 && fl_obj.lvl == 0) {
					let width = fl_obj.sib_arr[i].lvl_sum;
					let beg = lvl_cumu;
					let end = beg + width;
					lvl_cumu += width;
					beg = fl_xlate_x(lvl0_sum, beg);
					end = fl_xlate_x(lvl0_sum, end);
					fl_obj.sib_arr[i].beg = beg;
					fl_obj.sib_arr[i].end = end;
					//let dsum = 1.0e-9 * width;
					//let str = fl_obj.sib_arr[i].key;
					//ostr += "<br>flm rpt: be lvl= "+fl_obj.sib_arr[i].lvl+", str '"+str+"', sum= "+dsum+", beg= "+beg+", end= "+end;

				}
				if (1==1 && fl_obj.lvl > 0) {
					let width = fl_obj.sib_arr[i].lvl_sum;
					let beg = lvl_cumu;
					let end = beg + width;
					lvl_cumu += width;
					beg = fl_xlate_x(lvl0_sum, beg);
					end = fl_xlate_x(lvl0_sum, end);
					let rght_just_off = fl_xlate_x(lvl0_sum, lvl_tot);
					let base_beg = fl_obj.dad.beg;
					let base_end = fl_obj.dad.end;
					beg = base_end - rght_just_off + beg;
					end = base_end - rght_just_off + end;
					fl_obj.sib_arr[i].beg = beg;
					fl_obj.sib_arr[i].end = end;
					//let dsum = 1.0e-9 * width;
				}
				if (fl_obj.sib_arr[i].kids != null) {
					if (lvl_arr.length <= (lvl+1)) {
						lvl_arr.push([]);
					}
					let cs_str2 = cs_str+";"+fl_obj.sib_arr[i].key;
					if (fl_obj.sib_arr[i].sum > 0) {
						sum += fl_obj.sib_arr[i].sum;
					}
					lvl_arr[lvl+1] = [lvl+1, fl_obj.sib_arr[i].kids];
					build_fl_rpt(cs_idx, lvl_arr, lvl+1, cs_str2);
				} else  {
					if (1==20) {
					let str = cs_str+";"+fl_obj.sib_arr[i].key;
					let dsum = 1.0e-9 * fl_obj.sib_arr[i].sum;
					if (dsum > tm_cutoff) {
						//ostr += "<br>flm rpt: lvl= "+fl_obj.sib_arr[i].lvl+", str '"+str+"', sum= "+dsum;
						if (1==20) {
						// walk the parent list back to the base of this flame
						let f_o = fl_obj.sib_arr[i];
						let tms = 0;
						while (true) {
							if (f_o.dad == null) {break;}
							tms++;
							if (f_o.typ == 1) {
							ostr += "<br>flm rpt: lvl= "+f_o.lvl+", nm= "+f_o.key+", tms= "+tms;
							}
							f_o = f_o.dad;
							if (tms > 40) { break;}
						}
						}
					}
					}
					sum += fl_obj.sib_arr[i].sum;
				}
			}
		}
		let fl_menu_ch_title_str = "Flamegraph for event: "+g_fl_arr[file_tag_idx][cs_idx].event;
		g_fl_obj[file_tag_idx][cs_idx] = g_fl_obj_rt[file_tag_idx][cs_idx];
		if (g_fl_obj[file_tag_idx][cs_idx] == null) {
			console.log("apparently build_flame() never got any callstacks for cs_idx= "+cs_idx+", nothing to report");
			let fl_title_str = fl_menu_ch_title_str+", no data for graph";
			let file_tag = chart_data.file_tag;
			if (file_tag.length > 0) {
				file_tag = '<b>'+file_tag+'</b>&nbsp';
			} else {
				file_tag = '';
			}
			mytitle3.innerHTML = file_tag+fl_title_str;
			continue;
			//return;
		}
		let lvl_arr = [];
		let lvl = g_fl_obj[file_tag_idx][cs_idx].lvl;
		if (lvl_arr.length <= lvl) {
			lvl_arr.push([]);
		}
		lvl_arr[lvl] = [0, g_fl_obj[file_tag_idx][cs_idx]];
		build_fl_rpt(cs_idx, lvl_arr, lvl, "");
		//console.log("lvl0_sum: = "+ (1.0e-9 * lvl0_sum));
		let ctx3_range = {x_min: 0.0, x_max: 1.0, y_min: 0.0, y_max: -1.0};

		if (ctx3_range.y_max < 0.0) {
			ctx3_range.y_max = fl_lkup.length;
		}
		if (g_fl_arr[file_tag_idx][cs_idx].level0_tot == -1.0) {
			g_fl_arr[file_tag_idx][cs_idx].level0_tot = lvl0_sum;
		}

		function get_CPI(evt, f_o) {
			if (evt == "CPI") {
				let cycl = f_o.lvl_sum1;
				let inst = f_o.lvl_sum2;
				let cpi = 0;
				if (inst > 0) {
					cpi = cycl/inst;
				}
				return cpi;
			}
			if (evt == "GIPS") {
				let tm   = f_o.lvl_sum;
				let inst = f_o.lvl_sum2;
				let gips = 0;
				if (tm > 0) {
					gips = inst/tm;
				}
				return gips;
			}
			return 0;
		}
		function get_CPI_str(evt, f_o) {
			let cpi = get_CPI(evt, f_o);
			let tstr = ", "+evt+"= "+cpi.toFixed(3);
			return tstr;
		}


		let evt_idx_hash = {}
		evt_idx_hash["CPI"]  = 0;
		evt_idx_hash["GIPS"] = 1;
		let cpi_gradient_shape = [{}, {}];
		let cpi_gradient = [[], []];
		let cpi_hist_hsh = [{}, {}];
		let cpi_hist_vec = [[], []];
		let rgb_clr = [];
		let cpi_map_factor = [0.0, 0.0];
		let cpi_map_hi_2_rnk = [{}, {}];

		function build_cpi_gradient(evt)
		{
			let dbg_cpi_tm_tot = 0;
			if (typeof evt_idx_hash[evt] == 'undefined') {
				console.log("unexpected evt= "+evt);
				return;
			}
			let evt_idx = evt_idx_hash[evt];
			for (let i=0; i < fl_lkup.length; i++) {
				for (let j=0; j < fl_lkup[i].length; j++) {
					for (let k=0; k < fl_lkup[i][j].fl_obj.sib_arr.length; k++) {
						// don't have the check on zooming x here.
						// If we put it in then the colors 9for a given CPI) keeps changing as you zoom.
						let cpi = get_CPI(evt, fl_lkup[i][j].fl_obj.sib_arr[k]);
						let cpi_str = cpi.toFixed(2);
						if (typeof cpi_hist_hsh[evt_idx][cpi_str] == 'undefined') {
							cpi_hist_hsh[evt_idx][cpi_str] = cpi_hist_vec[evt_idx].length;
							let ncpi = Math.round(cpi*100)/100;
							if (ncpi == 0.0) {
								console.log("_fl cpi_zero= "+cpi_hist_vec[evt_idx].length);
							}
							cpi_hist_vec[evt_idx].push({cpi:ncpi, sum: 0, smpls:0, smpls1:0, smpls2:0, tm:0, cycl:0, inst:0});
						}
						let h_i = cpi_hist_hsh[evt_idx][cpi_str];
						if (typeof fl_lkup[i][j].fl_obj.sib_arr[k].cpi_idx == 'undefined') {
							fl_lkup[i][j].fl_obj.sib_arr[k].cpi_idx = [-1, -1];
						}
						fl_lkup[i][j].fl_obj.sib_arr[k].cpi_idx[evt_idx] = h_i;
						if (fl_lkup[i][j].fl_obj.sib_arr[k].sum > 0) {
							cpi_hist_vec[evt_idx][h_i].smpls++;
						}
						if (fl_lkup[i][j].fl_obj.sib_arr[k].sum1 > 0) {
							cpi_hist_vec[evt_idx][h_i].smpls1++;
						}
						if (fl_lkup[i][j].fl_obj.sib_arr[k].sum2 > 0) {
							cpi_hist_vec[evt_idx][h_i].smpls2++;
						}
						cpi_hist_vec[evt_idx][h_i].sum  += fl_lkup[i][j].fl_obj.sib_arr[k].sum;
						cpi_hist_vec[evt_idx][h_i].tm   += fl_lkup[i][j].fl_obj.sib_arr[k].sum;
						cpi_hist_vec[evt_idx][h_i].cycl += fl_lkup[i][j].fl_obj.sib_arr[k].sum1;
						cpi_hist_vec[evt_idx][h_i].inst += fl_lkup[i][j].fl_obj.sib_arr[k].sum2;
						//if ( fl_lkup[i][j].fl_obj.sib_arr[k].kids == null) {
							dbg_cpi_tm_tot += fl_lkup[i][j].fl_obj.sib_arr[k].sum;
						//}
					}
				}
			}
			let cpi_hist_lkup = [];
			for (let kk=0; kk < cpi_hist_vec[evt_idx].length; kk++) {
				cpi_hist_lkup.push(kk);
			}
			function sortHist(a, b) {
				return cpi_hist_vec[evt_idx][a].cpi - cpi_hist_vec[evt_idx][b].cpi;
			}
			function sortHistDesc(a, b) {
				return cpi_hist_vec[evt_idx][b].cpi - cpi_hist_vec[evt_idx][a].cpi;
			}
			if (evt == "CPI") {
				cpi_hist_lkup.sort(sortHist);
			} else if (evt == "GIPS") {
				cpi_hist_lkup.sort(sortHistDesc);
			}
			for (let kk=0; kk < cpi_hist_vec[evt_idx].length; kk++) {
				cpi_map_hi_2_rnk[evt_idx][cpi_hist_lkup[kk]] = kk;
			}
			let chv_sz = cpi_hist_vec[evt_idx].length;
			for (let kk=0; kk <= 255; kk++) {
				let str = vsprintf("#ff%02x00", [kk]);
				rgb_clr.push(str);
			}
			for (let kk=255; kk >= 0; kk--) {
				let str = vsprintf("#%02xff00", [kk]);
				rgb_clr.push(str);
			}
			for (let kk=0; kk <= 255; kk++) {
				let str = vsprintf("#00ff%02x", [kk]);
				rgb_clr.push(str);
			}
			for (let kk=255; kk >= 0; kk--) {
				let str = vsprintf("#00%02xff", [kk]);
				rgb_clr.push(str);
			}

			/*
			console.log("__cpi_tm_tot_= "+(1.0e-9 * dbg_cpi_tm_tot)+
					",cpi_hist_vec.len= "+cpi_hist_vec[evt_idx].length+
					",cpi_min= "+cpi_hist_vec[evt_idx][cpi_hist_lkup[1]].cpi+
					",cpi_max= "+cpi_hist_vec[evt_idx][cpi_hist_lkup[cpi_hist_vec[evt_idx].length-1]].cpi);
					*/
			if (cpi_hist_vec[evt_idx].length > 0) {
				cpi_map_factor[evt_idx] = 1024.0/cpi_hist_vec[evt_idx].length;
			}
			let cnvs_wd = mycanvas3.width - xPadding;
			let blk_wd = cnvs_wd / 1024;
			if (blk_wd < 1) { blk_wd = 1; }
			cpi_gradient_shape[evt_idx] = {x0:0, x1:0, y0:0, y1:0};
			for (let kk=0; kk <  cpi_hist_vec[evt_idx].length; kk++) {
				ctx3.beginPath();
				let nkk = Math.floor(kk * cpi_map_factor[evt_idx]);
				ctx3.fillStyle = rgb_clr[nkk];
				let x0= kk*blk_wd;
				ctx3.fillRect(x0, 0, blk_wd, 25);
				let cpi0 = cpi_hist_vec[evt_idx][cpi_hist_lkup[kk]].cpi;
				cpi_gradient[evt_idx].push({idx:kk, x0:x0, x1:(x0+blk_wd), y0:0, y1:25, cpi0:cpi0, cpi1:(cpi0+0.01)});
				cpi_gradient_shape[evt_idx].x1 = x0+blk_wd;
				cpi_gradient_shape[evt_idx].y1 = 25;

				//ctx3.strokeStyle = 'black';
				//ctx3.strokeRect(bg0[0], bg0[1], wd, hi);
			}
		}

		function fl_redraw(cs_idx, ctx_rng, do_draw) {
			let lvl0_sum = 0.0;
			let x_min = ctx_rng.x_min;
			let x_max = ctx_rng.x_max;
			let y_min = ctx_rng.y_min;
			let y_max = ctx_rng.y_max;
			if (do_draw) {
				//console.log("my3.h= "+mycanvas3.height);
				ctx3.clearRect(0, 0, mycanvas3.width, mycanvas3.height);
				ctx3.fillStyle = 'black';
			}
			let fl_ymx = fl_lkup.length;
			if (y_max < 0.0) {
				y_max = fl_ymx;
			}
			let got_y_max = 0.0;
			let evt_nm = g_fl_arr[file_tag_idx][cs_idx].event;
			let evt_idx = -1;
			if (evt_nm == "CPI" || evt_nm == "GIPS") {
				evt_idx = evt_idx_hash[evt_nm];
				build_cpi_gradient(evt_nm);
			}
			for (let i=0; i < fl_lkup.length; i++) {
				for (let j=0; j < fl_lkup[i].length; j++) {
					for (let k=0; k < fl_lkup[i][j].fl_obj.sib_arr.length; k++) {
						let x0 = fl_lkup[i][j].fl_obj.sib_arr[k].beg;
						let x1 = fl_lkup[i][j].fl_obj.sib_arr[k].end;
						let y0 = i;
						let y1 = i+1.0;
						if (x1 < x_min || x0 > x_max ) {
							continue;
						}
						if (got_y_max < y1) {
							got_y_max = y1;
						}
						if (x0 < x_min) {
							x0 = x_min;
						}
						if (x1 > x_max) {
							x1 = x_max;
						}
						if (do_draw) {
							if (fl_lkup[i][j].fl_obj.sib_arr[k].lvl == 0) {
								let diff_tot = fl_lkup[i][j].fl_obj.sib_arr[k].end - fl_lkup[i][j].fl_obj.sib_arr[k].beg;
								let diff_zm = x1 - x0;
								if (diff_tot > 0.0) {
									lvl0_sum += fl_lkup[i][j].fl_obj.sib_arr[k].lvl_sum * diff_zm/diff_tot;
								}
							}
							let bg0 = fl_xlate_ctx(mycanvas3, x0, y0, x_min, x_max, y_min, y_max);
							let bg1 = fl_xlate_ctx(mycanvas3, x1, y1, x_min, x_max, y_min, y_max);
							bg0[0] = Math.trunc(bg0[0]);
							bg0[1] = Math.trunc(bg0[1]);
							bg1[0] = Math.trunc(bg1[0]);
							bg1[1] = Math.trunc(bg1[1]);
							let wd = bg1[0] - bg0[0];
							let hi = bg1[1] - bg0[1];
							ctx3.beginPath();
							if (evt_nm != "CPI" && evt_nm != "GIPS") {
								ctx3.fillStyle = fl_lkup[i][j].fl_obj.sib_arr[k].color;
							} else {
								let h_i = fl_lkup[i][j].fl_obj.sib_arr[k].cpi_idx[evt_idx];
								if (cpi_hist_vec[evt_idx][h_i].cpi == 0.0) {
									ctx3.fillStyle = 'white';
								} else {
									let clr_idx = cpi_map_hi_2_rnk[evt_idx][h_i];
									//clr_idx *= cpi_map_factor;
									//clr_idx = Math.floor(clr_idx);
									let nkk = Math.floor(clr_idx * cpi_map_factor[evt_idx]);
									ctx3.fillStyle = rgb_clr[nkk];
								}
							}
							ctx3.fillRect(bg0[0], bg0[1], wd, hi);
							ctx3.strokeStyle = 'black';
							ctx3.strokeRect(bg0[0], bg0[1], wd, hi);
							if (fl_lkup[i][j].fl_obj.sib_arr[k].krnl == 1) {
								let triangle_wd = -hi;
								if (triangle_wd >= wd) {
									triangle_wd = wd;
								}
								ctx3.beginPath();
								ctx3.moveTo(bg0[0], bg1[1]);
								ctx3.lineTo(bg0[0], bg0[1]);
								ctx3.lineTo(bg0[0]+triangle_wd, bg1[1]);
								ctx3.closePath();

								// the outline
								//ctx3.lineWidth = 10;
								//ctx3.strokeStyle = '#666666';
								//ctx3.stroke();

								// the fill color
								ctx3.fillStyle = "red";
								ctx3.fill();
							}
							//txt_fld.innerHTML = "hi= " + hi+ ", font_sz= "+font_sz;
							let use_font_sz = font_sz;
							//let use_font_sz = 14;
							ctx3.font = use_font_sz + 'px Arial';
							/*
							if (-hi > (font_sz+1)) {
								use_font_sz = font_sz;
							} else if (-hi > (12+1)) {
								use_font_sz = 12;
							} else if (-hi > (10+1)) {
								use_font_sz = 10;
							}
							*/
							if (-hi >= (use_font_sz)) {
								let spc = (-hi - (font_sz))/2;
								//let tm = 1.0e-9 * fl_lkup[i][j].fl_obj.sib_arr[k].lvl_sum;
								let tm = fl_lkup[i][j].fl_obj.sib_arr[k].lvl_sum;
								let tm_str;
								if (evt_nm == "CPI" || evt_nm == "GIPS") {
									tm_str = tm_diff_str(tm, 3, cpi_str.time_unit);
								} else {
									tm_str = tm_diff_str(tm, 3, g_fl_arr[file_tag_idx][cs_idx].unit);
								}
								let tstr = fl_lkup[i][j].fl_obj.sib_arr[k].key + ", val= "+tm_str;
								if (evt_nm == "CPI" || evt_nm == "GIPS") {
									tstr += get_CPI_str(evt_nm, fl_lkup[i][j].fl_obj.sib_arr[k]);
								}
								let sz = ctx.measureText(tstr).width;
								ctx3.textAlign = "left";
								ctx3.fillStyle = 'black';
								if (sz <= wd) {
									ctx3.fillText(tstr, bg0[0]+1, bg0[1]-spc);
								} else if (wd > use_font_sz) {
									let str = tstr;
									while (str != "") {
										str = str.slice(0, -1);
										let sz = ctx.measureText(str).width;
										if (sz < wd) {
											ctx3.fillText(str, bg0[0]+1, bg0[1]-spc);
											break;
										}
									}
								}
							}
							ctx3.stroke();
						}
					}
				}
				//str += "<br>fl_lkup lvl["+i+"]: sum2= "+(1.0e-9 * sum2)+", sum2b= "+(1.0e-9 * sum2b)+", sum_sib= "+(1.0e-9*sum_sib)+", kids= "+kids;
			}
			let lvl0_str;
			if (evt_nm == "CPI" || evt_nm == "GIPS") {
				lvl0_str = tm_diff_str(g_fl_arr[file_tag_idx][cs_idx].level0_tot, 3, cpi_str.time_unit);
			} else {
				lvl0_str = tm_diff_str(g_fl_arr[file_tag_idx][cs_idx].level0_tot, 3, g_fl_arr[file_tag_idx][cs_idx].unit);
			}
			let zoom_str = "";
			if (g_fl_arr[file_tag_idx][cs_idx].level0_tot != lvl0_sum) {
				if (evt_nm == "CPI" || evt_nm == "GIPS") {
					zoom_str = ", zoomed to "+tm_diff_str(lvl0_sum, 3, cpi_str.time_unit);
				} else {
					zoom_str = ", zoomed to "+tm_diff_str(lvl0_sum, 3, g_fl_arr[file_tag_idx][cs_idx].unit);
				}
			}
			let file_tag = chart_data.file_tag;
			if (file_tag.length > 0) {
				file_tag = '<b>'+file_tag+'</b>&nbsp';
			}
			let unzoom_str = "";
			if (zoom_str == "") {
				unzoom_str = ". Level 0 covers "+lvl0_str;
			} else {
				unzoom_str = ". Unzoomed level 0 covers "+lvl0_str;
			}
			let fl_menu_ch_title_str = "Flamegraph for event: "+g_fl_arr[file_tag_idx][cs_idx].event;
			for (let kk=0; kk < lhs_menu_ch_list.length; kk++) {
				if (lhs_menu_ch_list[kk].title == fl_menu_ch_title_str) {
					if (lhs_menu_ch_list[kk].display_state == "hide") {
						let ele_nm = lhs_menu_ch_list[kk].hvr_nm;
						let ele = document.getElementById(ele_nm);
						if (ele != null) {
							ele.style.display = "none";
						}
					}
					break;
				}
			}
			let fl_title_str = fl_menu_ch_title_str;
			mytitle3.innerHTML = file_tag+fl_title_str+unzoom_str+zoom_str;
			//console.log("fl.ymx= "+got_y_max+", title= "+fl_title_str);
			return {y_max: got_y_max, lvl0_tot: lvl0_sum}
		}

		fl_redraw(cs_idx, ctx3_range, true);

		function fl_lkup_func(x, y, x_px, y_px, xm1, xp1) {
			let evt_nm = g_fl_arr[file_tag_idx][cs_idx].event;
			for (let i=0; i < fl_lkup.length; i++) {
				if (y < i || y > (1.0 + i)) {
					continue;
				}
				for (let j=0; j < fl_lkup[i].length; j++) {
					for (let k=0; k < fl_lkup[i][j].fl_obj.sib_arr.length; k++) {
						let x0 = fl_lkup[i][j].fl_obj.sib_arr[k].beg;
						let x1 = fl_lkup[i][j].fl_obj.sib_arr[k].end;
						let y0 = i;
						let y1 = i+1.0;
						if (x < x0 || x > x1) {
							if (!(x0 >= xm1 && x1 <= xp1)) {
							continue;
							}
						}
						let f_o = fl_lkup[i][j].fl_obj.sib_arr[k];
						let tms = 0;
						let tm = f_o.lvl_sum;
						let tm_str;
						if (evt_nm == "CPI" || evt_nm == "GIPS") {
							tm_str = tm_diff_str(tm, 3, cpi_str.time_unit);
						} else {
							tm_str = tm_diff_str(tm, 3, g_fl_arr[file_tag_idx][cs_idx].unit);
						}
						let cs_str3 = "x= "+x+", val= "+tm_str;
						if (evt_nm == "CPI" || evt_nm == "GIPS") {
							cs_str3 += get_CPI_str(evt_nm, f_o);
						}
						while (true) {
							if (f_o.dad == null) {break;}
							tms++;
							if (f_o.typ == 1) {
							cs_str3 += "<br>"+f_o.key;
							//ostr += "<br>flm rpt: lvl= "+f_o.lvl+", nm= "+f_o.key+", tms= "+tms;
							}
							f_o = f_o.dad;
							if (tms > 40) { break;}
						}
						setTooltipText(mytooltip3, mycanvas3, cs_str3, x_px, y_px);
						let str2 = "got it x0= "+x0+", x1= "+x1+", x= "+x+cs_str3;
						return {str2:str2, x0:x0, x1:x1, x:x+cs_str3};
						//let bg0 = fl_xlate_ctx(x0, y0, 0.0, 1.0, 0.0, fl_ymx);
						//let bg1 = fl_xlate_ctx(x1, y1, 0.0, 1.0, 0.0, fl_ymx);
					}
				}
				//str += "<br>fl_lkup lvl["+i+"]: sum2= "+(1.0e-9 * sum2)+", sum2b= "+(1.0e-9 * sum2b)+", sum_sib= "+(1.0e-9*sum_sib)+", kids= "+kids;
			}
			if (evt_nm == "CPI" || evt_nm == "GIPS") {
				let evt_idx = evt_idx_hash[evt_nm];
				//console.log("_x= "+x+",x_px= "+x_px);
				if (x_px >= cpi_gradient_shape[evt_idx].x0 && x_px <= cpi_gradient_shape[evt_idx].x1 &&
					y_px >= cpi_gradient_shape[evt_idx].y0 && y_px <= cpi_gradient_shape[evt_idx].y1) {
					for (let kk=0; kk < cpi_gradient[evt_idx].length; kk++) {
						if (cpi_gradient[evt_idx][kk].x0 <= x_px && x_px <= cpi_gradient[evt_idx][kk].x1) {
							let str= evt_nm+"= "+cpi_gradient[evt_idx][kk].cpi0 + " - " +cpi_gradient[evt_idx][kk].cpi1.toFixed(2);
							setTooltipText(mytooltip3, mycanvas3, str, x_px, y_px);
							return "";
						}
					}
				}
			}
			clearToolTipText(mytooltip3);
			current_tooltip_text = "";
			current_tooltip_shape = -1;
			return "";
		}

		mycanvas3.onmousemove = function(e) {
			if ( typeof can_shape.hvr_prv_x == 'undefined' ) {
				can_shape.hvr_prv_x == -1;
				can_shape.hvr_prv_y == -1;
			}
			// important: correct mouse position:
			let rect = this.getBoundingClientRect(),
				//x = Math.trunc(e.clientX - rect.left),
				//y = Math.trunc(e.clientY - rect.top);
				x = e.clientX - rect.left,
				y = e.clientY - rect.top;
			if (x != can_shape.hvr_prv_x || y != can_shape.hvr_prv_y) {
				can_shape.hvr_prv_x = x;
				can_shape.hvr_prv_y = y;
				let uchi = mycanvas3.height-0.0;
				let ucwd = mycanvas3.width;
				let xn   = ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * x / ucwd;
				let xnm1 = ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * (x-2.0) / ucwd;
				let xnp1 = ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * (x+2.0) / ucwd;
				let yn   = ctx3_range.y_min + (ctx3_range.y_max - ctx3_range.y_min) * (uchi - y)/uchi;
				let ostr2 = fl_lkup_func(xn, yn, x, y, xnm1, xnp1);
				//mycanvas3_txt.innerHTML = "_x= "+x+", y= "+y+", xn= "+xn+", yn= "+yn+", str2= "+ostr2.str2;
				return;
			}
		};

		mycanvas3.onmousedown = function (e) {
			let xPadding = 0.0;
			let yPadding = 2.0;
			let rect = this.getBoundingClientRect(),
				//x = Math.trunc(e.clientX - rect.left - xPadding),
				//y = Math.trunc(e.clientY - rect.top);
				x = (e.clientX - rect.left - xPadding),
				y = (e.clientY - rect.top);
			ms_dn_pos = [x, y];
			mycanvas3.onmouseup = function (evt) {
				let rect = this.getBoundingClientRect(),
					//x = Math.trunc(evt.clientX - rect.left - xPadding),
					//y = Math.trunc(evt.clientY - rect.top);
					x = (evt.clientX - rect.left - xPadding),
					y = (evt.clientY - rect.top);
				let x0 = 1.0 * ms_dn_pos[0];
				let x1 = 1.0 * x;
				let xdiff0= ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * x0 / (px_wide - xPadding);
				let xdiff1= ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * x1 / (px_wide - xPadding);
				let nx0 = +minx+( xdiff0 * (maxx - minx));
				//console.log("x0= "+x0+", rec.right= "+rect.right+", maxx= "+maxx+", minx= "+minx+", xdiff0= "+xdiff0+", nx0= "+nx0);
				let nx1 = +minx+(xdiff1 * (maxx - minx));
				//console.log("x_px_diff= "+(x - ms_dn_pos[0])+", nd1= "+xdiff0+", xd1= "+xdiff1+", xdff="+(xdiff1-xdiff0));
				if (Math.abs(x - ms_dn_pos[0]) <= 3) {
					console.log("Click_2 "+", btn= "+evt.button);
					nx0_prev = nx0;
				can_shape.hvr_prv_x = x;
				can_shape.hvr_prv_y = y;
				let uchi = mycanvas3.height-0.0;
				let ucwd = mycanvas3.width;
				let xn   = ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * x / ucwd;
				let xnm1 = ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * (x-2.0) / ucwd;
				let xnp1 = ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * (x+2.0) / ucwd;
				let yn   = ctx3_range.y_min + (ctx3_range.y_max - ctx3_range.y_min) * (uchi - y)/uchi;
				let ostr2 = fl_lkup_func(xn, yn, x, y, xnm1, xnp1);
				console.log("str2="+ostr2.str2);
				if (typeof ostr2 != 'undefined' && typeof ostr2.x0 != 'undefined') {
					ctx3_range = {x_min:ostr2.x0, x_max:ostr2.x1, y_min:0.0, y_max:-1.0};
					let ret = fl_redraw(cs_idx, ctx3_range, false);
					ctx3_range.y_max = ret.y_max;
					mycanvas3.height = ret.y_max * (font_sz+1.02);
					console.log("ymx= "+ret.y_max+",font_sz= "+font_sz);
					fl_redraw(cs_idx, ctx3_range, true);
				}
				} else {
					console.log("mouse drag");
					if (xdiff0 > xdiff1) {
						ctx3_range = {x_min:0, x_max:1.0, y_min:0.0, y_max:-1.0};
						//let ret = fl_redraw(cs_idx, ctx3_range, false);
						//ctx3_range.y_max = ret.y_max;
					} else {
						ctx3_range = {x_min:xdiff0, x_max:xdiff1, y_min:0.0, y_max:-1.0};
						//ctx3_range.y_max = fl_redraw(cs_idx, ctx3_range, false);
					}
					let ret = fl_redraw(cs_idx, ctx3_range, false);
					ctx3_range.y_max = ret.y_max;
					mycanvas3.height = ret.y_max * (font_sz+1.02);
					console.log("ymx= "+ret.y_max+",font_sz= "+font_sz);
					fl_redraw(cs_idx, ctx3_range, true);
				}
				mycanvas3.offmouseup = null;
				mycanvas3.scrollIntoView(false);
			};
		};

		tsum += sum;
		//console.log("sum0 = "+(1.0e-9 * sum0)+", fl_end_sum= "+(1.e-9 * fl_end_sum));
		}
		return tsum;
	}

	function canvas_px_high(this_ele) {
		if (this_ele == null) {
			if (typeof px_high_in == 'undefined' || px_high_in < 100) {
				return gpixels_high_default;
			}
			return px_high;
		} else {
			return this_ele.height;
		}
	}

	tot_line.divisions = 100.0;
	tot_line.xdelta    = (maxx - minx) / tot_line.divisions;
	function tot_line_get_values()
	{
		let ret_data = {};
		let maxy_new = maxy;
		ret_data = {maxy_new: maxy_new, typ: "def"};
		if (tot_line.evt_str.length == 0) {
			return ret_data;
		}
		maxy_new = null;
		let fld_1st_time = {};
		let HT_enabled = false;
		let desc = "";
		let map_num_den = 0;
		let scope = [];
		if (typeof chart_data.tot_line_opts_has_num_den !== 'undefined') {
			map_num_den = +chart_data.tot_line_opts_has_num_den;
		}
		if (typeof chart_data.tot_line_opts_desc !== 'undefined') {
			desc = chart_data.tot_line_opts_desc;
		}
		if (desc != "") {
			desc += "\n";
		}
		desc += "See chart: " + chart_data.title;
		desc += "\n" + "chart_tag: " + chart_data.chart_tag;
		if (typeof chart_data.tot_line_opts_xform !== 'undefined' &&
			chart_data.tot_line_opts_xform == 'map_cpu_2_core') {
			// I probably should have a way to indicate if really want to divide by 2 for HT
			// The 'div by 2' is for the case where we are computing cycles/uop with HT enabled
			// then summing core thread 0 and 1.
			for(let cpu=0; cpu < chart_data.map_cpu_2_core.length; cpu++) {
				if (chart_data.map_cpu_2_core[cpu] != cpu) {
					HT_enabled = true;
					break;
				}
			}
		}
		let HT_factor_num = 1.0, HT_factor_den = 1.0;
		if (typeof chart_data.tot_line_opts_scope !== 'undefined') {
			scope = chart_data.tot_line_opts_scope;
		}
		if (scope.length > 0) {
			if (HT_enabled) {
				if (scope.length >= 1 && scope[0] == 'per_core') {
					HT_factor_num = 0.5;
				}
				if (scope.length >= 2 && scope[1] == 'per_core') {
					HT_factor_den = 0.5;
				}
				//console.log(sprintf("n= %.1f, d= %.1f mnd= %d ttl= %s", HT_factor_num, HT_factor_den, map_num_den, chart_data.title));
			} else {
				HT_factor_num = 1.0;
				HT_factor_den = 1.0;
			}
		}
		let num=0.0, den=0.0;
		if (ch_type == "line" || ch_type == "stacked") {
			tot_line.xarray.length = tot_line.divisions+1;
			for (let sci= 0; sci < tot_line.evt_str.length; sci++) {
				tot_line.xarray2[sci].length = tot_line.divisions+1;
				tot_line.yarray2[sci].length = tot_line.divisions+1;
				tot_line.yarray[sci].length = tot_line.divisions+1;
				//console.log(chart_data.map_cpu_2_core);
				for (let j=0; j < tot_line.yarray[sci].length; j++) {
					tot_line.yarray[sci][j] = 0.0;
					tot_line.yarray2[sci][j] = 0.0;
					tot_line.xarray2[sci][j] = 0.0;
					tot_line.xarray[j] = 0.0;
				}
				for (let i=0; i < chart_data.myshapes.length; i++) {
					let x0 = chart_data.myshapes[i].pts[PTS_X0];
					let x1 = chart_data.myshapes[i].pts[PTS_X1];
					let cpu = chart_data.myshapes[i].ival[IVAL_CPU];
					if (tot_line.evt_str_base_val_arr.length > 0) {
						if (typeof chart_data.tot_line_opts_xform !== 'undefined' &&
							chart_data.tot_line_opts_xform == 'map_cpu_2_core') {
							let core = chart_data.map_cpu_2_core[cpu];
							if (tot_line.evt_str_base_val_arr[sci] != core) {
								continue;
							}
						}
					}
					if (x1 < minx || x0 > maxx) {
						continue;
					}
					let tx0 = x0;
					let tx1 = x1;
					if (x0 < minx) {
						tx0 = minx;
					}
					if (x1 > maxx) {
						tx1 = maxx;
					}
					let y0 = chart_data.myshapes[i].pts[PTS_Y0];
					let y1 = chart_data.myshapes[i].pts[PTS_Y1];
					let yval, num, den;
					if (ch_type == "line") {
						yval = y1;
						if (map_num_den > 0) {
							num = chart_data.myshapes[i].num;
							den = chart_data.myshapes[i].den;
						}
					} else {
						yval = (y1 - y0);
					}
					let relx0 = tx0 - minx;
					let relx1 = tx1 - minx;
					relx0 /= (maxx - minx);
					relx1 /= (maxx - minx);
					let nrx0 = tot_line.divisions * relx0;
					let nrx1 = tot_line.divisions * relx1;
					let xbeg = Math.floor(nrx0);
					let xend = Math.ceil(nrx1);
					let ibeg = Math.trunc(xbeg);
					let iend = Math.trunc(xend);
					//console.log("i= "+i+", beg= "+ibeg+", end= "+iend);
					for (let j=ibeg; j < iend; j++) {
						let xcur0, xcur1;
						if (j == ibeg) {
							xcur0 = nrx0/tot_line.divisions;
						} else {
							xcur0 = j/tot_line.divisions;
						}
						if ((j+1) == iend) {
							xcur1 = nrx1/tot_line.divisions;
						} else {
							xcur1 = (j+1)/tot_line.divisions;
						}
						if (map_num_den > 0) {
							tot_line.yarray2[sci][j] += HT_factor_num * num * (xcur1 - xcur0) * tot_line.divisions;
							tot_line.xarray2[sci][j] += HT_factor_den * den * (xcur1 - xcur0) * tot_line.divisions;
							tot_line.yarray[sci][j] = tot_line.yarray2[sci][j]/tot_line.xarray2[sci][j];
						} else {
							tot_line.yarray[sci][j] += yval * (xcur1 - xcur0) * tot_line.divisions;
						}
					}
				}
				let tot_avg = 0, tot_avg2= 0;
				let tot_avg2_num = 0;
				let tot_avg2_den = 0;
				for (let j=0; j < tot_line.divisions+1; j++) {
					tot_line.xarray[j] = minx + j * (maxx - minx) / tot_line.divisions;
					tot_avg += tot_line.yarray[sci][j];
					tot_avg2_num += tot_line.yarray2[sci][j];
					tot_avg2_den += tot_line.xarray2[sci][j];
					if (maxy_new == null || maxy_new <= tot_line.yarray[sci][j]) {
						maxy_new = tot_line.yarray[sci][j];
						ret_data = {maxy_new: maxy_new, typ: "ck"}
					}
				}
				if (tot_line.divisions > 0) {
					tot_avg /= tot_line.divisions;
					tot_avg2 = tot_avg2_num/tot_avg2_den;
					//console.log(sprintf("tot_avg[%d]= %.3f title= %s\n", sci, tot_avg, chart_data.title));
					if (g_cpu_diagram_flds != null) {
						for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
							if (chart_data.chart_tag == g_cpu_diagram_flds.cpu_diagram_fields[j].chart) {
								if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr === 'undefined'
									|| typeof fld_1st_time[j] === 'undefined') {
									g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr = [];
									fld_1st_time[j] = 1;
								}
								if (map_num_den == 0) {
									g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.push(tot_avg);
								} else {
									g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.push(tot_avg2);
								}
								let y_fmt = "%.3f";
								if (typeof chart_data.tot_line_opts_yval_fmt !== 'undefined') {
									y_fmt = chart_data.tot_line_opts_yval_fmt;
								}
								g_cpu_diagram_flds.cpu_diagram_fields[j].y_label = chart_data.y_label;
								g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_fmt = y_fmt;
								g_cpu_diagram_flds.cpu_diagram_fields[j].desc = desc;
								g_cpu_diagram_flds.cpu_diagram_fields[j].chart_tag = chart_data.chart_tag;
								g_cpu_diagram_flds.cpu_diagram_fields[j].map_cpu_2_core = chart_data.map_cpu_2_core;
								//console.log("got cpu_diagram chart match= "+chart_data.chart_tag);
							}
						}
					}
				}
			}
			//console.log(tot_line.xarray);
			//console.log(tot_line.yarray);
		}
		return ret_data;
	}

	function draw_rect(ctx_in, x, y, wd, hi, clr, yPxlzero, do_highlight)
	{
		//ctx_in.rect(x-wd/2, y-wd/2, wd, hi);
		ctx_in.fillStyle = clr;
		let hi2 = y+hi;
		let diff = 0;
		if (hi2 > yPxlzero) {
			diff = 0.5*(hi2 - yPxlzero);
		}
		let x0=x-wd/2;
		let y0=y-hi/2;
		let x1=x0+wd;
		let y1=y0+hi-diff;
		//ctx_in.fillRect(x-wd/2, y-hi/2, wd, hi-diff);
		ctx_in.fillRect(x0, y0, x1-x0, y1-y0);
		let rct = {x0:x0, y0:y0, x1:x1, y1:y1};
		if (do_highlight) {
			let str_style = ctx_in.strokeStyle;
			let str_lw    = ctx_in.lineWidth;
			ctx_in.strokeStyle = 'black';
			ctx_in.lineWidth=2;
			ctx_in.strokeRect(x0, y0, x1-x0, y1-y0);
			ctx_in.strokeStyle = str_style;
			ctx_in.lineWidth   = str_lw;
		}
		//console.log(rct);
		return rct;
	}

	function chart_redraw(from_where) {
		let tl_maxy_new = tot_line_get_values();
		let maxy_new = tl_maxy_new.maxy_new;
		redo_ylkup(ctx);
		let build_fl_tm = 0.0;
		let tm_here_04a = performance.now();
		let displayed_lines= 0;
		ctx.clearRect(0, 0, mycanvas.width, mycanvas.height);
		// draw yaxis label
		let step = [];
		ctx.fillStyle = 'black';
		ctx.save();
		ctx.translate(0, 0);
		ctx.rotate(-Math.PI/2);
		//ctx.textAlign = "right";
		ctx.textAlign = "center";
		//lineHeight = 15; // this is guess and check as far as I know
		ctx.font = font_sz + 'px Arial';
		//let str = chart_data.y_label + " by " + chart_data.y_by_var;
		let str = chart_data.y_label;
		//let str = chart_data.title;
		ctx.fillText(str, -canvas_px_high(null)/2, font_sz);
		ctx.restore()
		// draw y by_var values.
		ctx.font = font_sz + 'px Arial';
		ctx.textAlign = "right";
		let uminy = miny;
		let umaxy = tl_maxy_new.maxy_new;
		if (ch_type == "line" || ch_type == "stacked") {
			let tminy=null, tmaxy=null;
			if (chart_data.chart_tag == "WAIT_TIME_BY_proc" ||
					chart_data.chart_tag == "RUN_QUEUE" ||
					chart_data.chart_tag == "RUN_QUEUE_BY_CPU") {
				g_fl_obj[file_tag_idx]= {};
				g_fl_obj_rt[file_tag_idx] = {};
			}
			let dbg_cnt=0, ck_it=0;
			let got_ymx = {};
			for (let i=0; i < chart_data.myshapes.length; i++) {
				let x0 = chart_data.myshapes[i].pts[PTS_X0];
				let x1 = chart_data.myshapes[i].pts[PTS_X1];
				if (x1 < minx || x0 > maxx) {
					continue;
				}
				let fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
				let do_event = false;
				if (ck_it == 1 && ++dbg_cnt < 10) {
					//console.log("e_se["+fe_idx+"]="+ event_select[fe_idx][1]);
					//console.log("got RAPL chart");
				}
				if (fe_idx == -1 || (typeof event_select[fe_idx] != 'undefined' &&
					(event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show'))) {
					do_event = true;
				}
				if (!do_event) {
					continue;
				}
				let y0 = chart_data.myshapes[i].pts[PTS_Y0];
				let y1 = chart_data.myshapes[i].pts[PTS_Y1];
				if (tmaxy == null) {
					tminy = y0;
					tmaxy = tminy;
					got_ymx.x0 = x0;
					got_ymx.x1 = x1;
					got_ymx.y0 = y0;
					got_ymx.y1 = y1;
				}
				if (tminy > y0) { tminy = y0; }
				if (tminy > y1) { tminy = y1; }
				if (tmaxy < y0) {
					tmaxy = y0;
					got_ymx.x0 = x0;
					got_ymx.x1 = x1;
					got_ymx.y0 = y0;
					got_ymx.y1 = y1;
				}
				if (tmaxy < y1) {
					tmaxy = y1;
					got_ymx.x0 = x0;
					got_ymx.x1 = x1;
					got_ymx.y0 = y0;
					got_ymx.y1 = y1;
				}
			}
			if (tmaxy != null) {
				if (tminy == tmaxy) {
					tmaxy *= 1.05;
					tminy *= 0.95;
				}
				uminy = tminy;
				umaxy = tmaxy;
				if (umaxy < maxy_new && tl_maxy_new.typ != "def") {
					umaxy = tl_maxy_new.maxy_new;
				}
			}
			if (chart_data.title == "freq from cycles (GHz) by cpu" ||
				chart_data.title == "L2 accesses (Mill_accesses/s) from r16 L2 dcache accesses by cpu") {
				//console.log(sprintf("++mxy for freq from ccyles: x0= %s x1= %s, y0= %s, y1= %s, umaxy= %s, maxy_new= %s, maxy= %s, tl_maxy_new= %s, ttl= %s",
				//			got_ymx.x0, got_ymx.x1, got_ymx.y0, got_ymx.y1, umaxy, maxy_new, maxy, JSON.stringify(tl_maxy_new), chart_data.title));
			}
			let x   = xPadding - 5;
			let y   = font_sz
			let str;
			if (chart_data.y_fmt != "") {
				str = vsprintf(chart_data.y_fmt, [umaxy]);
			} else {
				str = umaxy.toFixed(y_axis_decimals);
			}
			ctx.fillText(str, x, y);
			y   = canvas_px_high(null) - yPadding;
			if (chart_data.y_fmt != "") {
				str = vsprintf(chart_data.y_fmt, [uminy]);
			} else {
				str = uminy.toFixed(y_axis_decimals);
			}
			ctx.fillText(str, x, y);
		} else {
			g_fl_obj[file_tag_idx]= {};
			g_fl_obj_rt[file_tag_idx] = {};
			for (let i=0; i < ylkup.length; i++) {
				let x   = xPadding - 5;
				let y   = ylkup[i][2];
				let idx = ylkup[i][4];
				if (chart_data.subcat_rng[idx].subcat != 0) {
					continue;
				}
				let str = chart_data.y_by_var + " " + chart_data.subcat_rng[idx].cat_text;
				ctx.fillText(str, x, y);
			}
		}
		let beg = xlate(ctx, minx, 0, minx, maxx, uminy, umaxy);
		ctx.textAlign = "left";
		let tstr = "rel.T= "+minx;
		ctx.fillText(tstr, beg[0], canvas_px_high(null) - yPadding + font_sz);
		tstr = "T= "+(minx + chart_data.ts_initial.ts - chart_data.ts_initial.ts0x);
		ctx.fillText(tstr, beg[0], canvas_px_high(null) - yPadding + 2* font_sz);
		beg = xlate(ctx, maxx, 0, minx, maxx, uminy, umaxy);
		ctx.textAlign = "right";
		tstr = "rel.T= "+maxx;
		ctx.fillText(tstr, beg[0], canvas_px_high(null) - yPadding + font_sz);
		tstr = "T= "+(maxx + chart_data.ts_initial.ts - chart_data.ts_initial.ts0x);
		ctx.fillText(tstr, beg[0], canvas_px_high(null) - yPadding + 2* font_sz);
		tstr = "delta T= "+tm_diff_str(maxx - minx, 6, 'secs');
		ctx.fillText(tstr, beg[0], canvas_px_high(null) - yPadding + 3* font_sz);
		// draw yaxis line
		ctx.beginPath();
		ctx.moveTo(xPadding-1, 0);
		ctx.lineTo(xPadding-1, canvas_px_high(null) - yPadding+1);
		ctx.stroke();
		// draw xaxis line
		ctx.beginPath();
		ctx.moveTo(xPadding-1, canvas_px_high(null) - yPadding+1);
		ctx.lineTo(px_wide, canvas_px_high(null)-yPadding+1);
		//ctx.lineTo(0, canvas_px_high(null) - yPadding+5);
		//ctx.lineTo(mycanvas.wide, 0);
		ctx.stroke();
		let line_done = [];
		lkup = [];
		for (let i=0; i < chart_data.subcat_rng.length; i++) {
			line_done.push({});
			lkup.push([]);
			step.push([-1, -1]);
		}
		let filtering = false;
		if (ch_type != "line" && ch_type != "stacked") {
			for (let j=0; j < proc_arr.length; j++) {
				let i = proc_arr[j][1];
				if (proc_select[i][1] == 'hide') {
					filtering = true;
					break;
				}
			}
		}
		let skipped=0, drew_lns=0, drew_bxs=0;
		let cs_str_period = 0;
		let cs_sum = 0;
		let dbg_cntr = 0;
		let fl_tm_bld = 0;
		let fl_tm_rpt = 0;
		for (let i=0; i < chart_data.myshapes.length; i++) {
			let x0 = chart_data.myshapes[i].pts[PTS_X0];
			let x1 = chart_data.myshapes[i].pts[PTS_X1];
			let y0 = chart_data.myshapes[i].pts[PTS_Y0];
			let y1 = chart_data.myshapes[i].pts[PTS_Y1];
			let operiod = chart_data.myshapes[i].ival[IVAL_PERIOD];
			let cat = chart_data.myshapes[i].ival[IVAL_CAT];
			let subcat = chart_data.myshapes[i].ival[IVAL_SUBCAT];
			let subcat_idx = subcat_cs_2_sidx_hash[cat][subcat];
			if (x0 == x1 && ch_type == "block") {
				/* this is the case of the samples on kernelshark chart: x0==x1
				 * the flamegrphs are built from these 'shapes'/events
				 * If you are zooming in, say for sched_switch, you might get no events
				 * But for some events (like sched_switch) I know the beg and end of the event.
				 * because there isn't a context switch during the interval.
				 * For instance, you might not get a cswitch for 1+ seconds whereas you
				 * might zoom into usec level. Since the cswitch overlaps this zoom interval
				 * the callstack (the actual cswitch) might be out of the zoom range.
				 * So I compute the interval and crop it to the zoom range (if it is in range).
				 * This is sort of fake... the context switch or cpu-clock callstack
				 * might have actually occurred out of the range.
				 * I could probably do this for more events and just take the percent of
				 * the sample that lies in the interval... but it is harder to say what the
				 * interval really is. One could say the 'interval' is the time delta
				 * between events but that is kind of fuzzy. For instance, if the cpu has gone
				 * idle then there might a long interval without samples.
				 */
				if (x0 < minx) {
					continue;
				}
				let fe_idx = chart_data.myshapes[i].ival[IVAL_FE];
                if (fe_idx < 0 || typeof fe_idx == 'undefined') {
                    //console.log("fe_idx= "+fe_idx);
                }
				let fe_2 = event_lkup[fe_idx];
                if (fe_2 < 0 || typeof fe_2 == 'undefined') {
                    //console.log("fe_2= "+fe_2);
                }
				let ev = "unknown3";
                if (typeof fe_2 != 'undefined') {
					ev = event_list[fe_2].event;
					if (ev != "CSwitch" && ev != "sched:sched_switch" && ev != "cpu-clock") {
						if (x1 > maxx) {
							continue;
						}
					}
                }
				let nperiod = operiod;
				if (ev == "cpu-clock") {
					nperiod *= 1.0e-9; // cpu-clock period is in ns
				}
				let x0t = x1 - nperiod;
				if (x0t > maxx) {
					continue;
				}
				/*
				if (x0 > maxx) {
					continue;
				}
				*/
				if (x0t < minx) {
					x0t = minx;
				}
				let x1t = x1;
				if (x1t > maxx) {
					x1t = maxx;
				}
				operiod = x1t - x0t;
				if (ev == "cpu-clock") {
					operiod = operiod * 1e9;
				}
				x0t = x1t;
			} else {
				if (x1 < minx || x0 > maxx) {
					continue;
				}
				if (x0 < minx) {
					x0 = minx;
				}
				if (x1 > maxx) {
					x1 = maxx;
				}
			}
			//console.log("subcat_idx= "+subcat_idx+", cat= "+cat+", subcat= "+subcat);
			let beg = xlate(ctx, x0, y0, minx, maxx, uminy, umaxy);
			let end = xlate(ctx, x1, y1, minx, maxx, uminy, umaxy);
			if (beg[0] < xPadding) {
				beg[0] = xPadding;
			}
			if (end[0] > px_wide) {
				end[0] = px_wide;
			}
			let idx = chart_data.myshapes[i].ival[IVAL_CPT];
			beg[0] = Math.trunc(beg[0]);
			beg[1] = Math.trunc(beg[1]);
			end[0] = Math.trunc(end[0]);
			end[1] = Math.trunc(end[1]);
			let drew_this_shape = false;
			if (ch_type == 'block' && chart_data.myshapes[i].ival[IVAL_SHAPE] == SHAPE_RECT) {
				let wd = end[0] - beg[0];
				let hi = end[1] - beg[1];
				if (wd < 0) {
					console.log("wd= "+wd);
				}
				//ctx.fillRect(beg[0],beg[1], wd, hi);
				ctx.beginPath();
				drew_bxs++;
				if (idx >= 0) {
					let rnk = proc_rank[idx];
					if (typeof proc_select[idx] != 'undefined' &&
						(proc_select[idx][1] == 'highlight' || proc_select[idx][1] == 'show')) {
						if (rnk < number_of_colors_proc) {
							ctx.fillStyle = gcolor_lst[rnk];
							ctx.fillRect(beg[0]-1,beg[1], wd+2, hi);
						} else {
							ctx.fillStyle = gcolor_def; // lightgrey for events above 'number of colors' rank
							ctx.fillRect(beg[0]-1,beg[1], wd+2, hi);
						}
						if (proc_select[idx][1] == 'highlight') {
							ctx.lineWidth=5;
							ctx.strokeStyle = 'black';
							ctx.strokeRect(beg[0]-1,beg[1], wd+2, hi);
							ctx.stroke();
							ctx.lineWidth=1;
						}
						drew_this_shape = true;
					}
				} else {
					ctx.rect(beg[0]-1,beg[1], wd+2, hi);
					ctx.stroke();
				}
				//console.log("x0= "+beg[0]+", y0= "+beg[1]+", width= "+wd+", hi= "+hi);
			} else {
				let fe_idx;
				if (ch_type == "line") {
					fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
				} else if (ch_type == "stacked") {
					fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
					//fe_idx = chart_data.myshapes[i].ival[IVAL_FE];
					//let fe_2 = event_lkup[fe_idx];
				} else {
					fe_idx = chart_data.myshapes[i].ival[IVAL_FE];
					if (fe_idx < 0 || fe_idx >= event_lkup.length) {
						console.log("got fe_idx= "+fe_idx+" event_.sz= "+event_list.length);
					}
					let fe_2 = event_lkup[fe_idx];
					//console.log("got fe_idx= "+fe_2+" flnm_evt.sz= "+event_list.length);
				}
				let fe_rnk = event_lkup[fe_idx];
				//console.log("fe_idx= "+fe_idx+", fe_rnk= "+fe_rnk);
				let clr = gcolor_def;
                if (typeof fe_rnk != 'undefined') {
				    clr = event_list[fe_rnk].color;
                }
				let do_proc = false, do_event = false, do_event_highlight = false;
				if (idx == -1 || (typeof proc_select[idx] != 'undefined' &&
					(proc_select[idx][1] == 'highlight' || proc_select[idx][1] == 'show'))) {
					do_proc = true;
				}
				if (fe_idx == -1 || (typeof event_select[fe_idx] != 'undefined' &&
					(event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show'))) {
					do_event = true;
					if (event_select[fe_idx][1] == 'highlight') {
						do_event_highlight = true;
					}
				}
				if ((do_proc && ch_type == "block") || (do_event &&
						(chart_data.chart_tag == "WAIT_TIME_BY_proc" ||
						 chart_data.chart_tag == "RUN_QUEUE" ||
						 chart_data.chart_tag == "RUN_QUEUE_BY_CPU"))) {
					let tm_0 = performance.now();

					if (chart_data.chart_tag == "WAIT_TIME_BY_proc" ||
							chart_data.chart_tag == "RUN_QUEUE" ||
							chart_data.chart_tag == "RUN_QUEUE_BY_CPU") {
						fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
					}
					let fe_2 = event_lkup[fe_idx];
					//console.log("got fe_idx= "+fe_2+" flnm_evt.sz= "+event_list.length);
					let ev = "unknown4";
                    if (typeof fe_2 != 'undefined') {
					    ev = event_list[fe_2].event;
                    }
					let has_cs = false;
					if (typeof chart_data.myshapes[i].cs_strs != 'undefined'
						&& chart_data.myshapes[i].cs_strs.length > 0) {
							has_cs = true;
					}
					if (fe_2 < event_list.length && has_cs) {
						let cs_clr = gcolor_def;
						let tm_fl_here_0 = performance.now();
						let cpt= chart_data.myshapes[i].ival[IVAL_CPT];
						if (cpt >= 0) {
							let rnk = proc_rank[cpt];
							if (rnk < number_of_colors_proc) {
								cs_clr =  gcolor_lst[rnk];
							}
						}
						let unit = ev;
						if (chart_data.chart_tag == "WAIT_TIME_BY_proc" ||
								chart_data.chart_tag == "RUN_QUEUE") {
							operiod = y1;
							operiod *= 1e9;
							unit = 'nsecs';
						}
						if ( chart_data.chart_tag == "RUN_QUEUE_BY_CPU") {
							operiod = y1;
							//operiod *= 1e9;
							unit = 'pct';
						}
						let period = operiod;

						if (ev == "sched:sched_switch" || ev == "CSwitch") {
							// for sched_switch, the 'period' is the duration in seconds. convert secs to nanaosecs.
							// The other events (like cycles and instructions) have about 1e9 occurences/sec so
							// using ns for time gives a similar 'occurences/sec'.
							if ((x1-period) < minx) {
								let x0t = x1-period;
								period -= (minx - x0t);
							}
							period *= 1e9;
							unit = 'nsecs'; // see tm_diff_str() for supported units
						} else if (ev == "SampledProfile") {
							// the default SampledProfile interval seems to 1 millisec. convert secs to nanaosecs.
							// The other events (like cycles and instructions) have about 1e9 occurences/sec so
							// using ns for time gives a similar 'occurences/sec'.
								// period is sample count so convert to seconds
								period = 0.001;
							period *= 1e9; // and then to nsecs
							//unit = 'msecs'; // see tm_diff_str() for supported units
							unit = 'nsecs'; // see tm_diff_str() for supported units
						} else {
							if (!(chart_data.chart_tag == "WAIT_TIME_BY_proc" ||
										chart_data.chart_tag == "RUN_QUEUE" ||
										chart_data.chart_tag == "RUN_QUEUE_BY_CPU")) {
								period = chart_data.myshapes[i].ival[IVAL_PERIOD];
							}
						}
						if (ev == 'cpu-clock' || ev == 'task-clock') {
							unit = 'nsecs'; // see tm_diff_str() for supported units
						}
						cs_str_period += period;
						let nm = "unknown2";
						let comm, comm_pid;
						let doing_idle = false;
						if (cpt >= 0 && chart_data.proc_arr[cpt].tid > -1) {
							let comm = chart_data.proc_arr[cpt].comm;
							let pid  = chart_data.proc_arr[cpt].pid;
							let tid  = chart_data.proc_arr[cpt].tid;
							nm = comm+" "+pid+"/"+tid;
							if (pid == 0 && tid == 0) {
								doing_idle = true;
							}
							comm = chart_data.proc_arr[cpt].comm;
							comm_pid = comm+" "+chart_data.proc_arr[cpt].pid;
							if (g_flamegraph_base == FLAMEGRAPH_BASE_CP) {
								nm = comm_pid;
							}
							if (g_flamegraph_base == FLAMEGRAPH_BASE_C) {
								nm = comm;
							}
						}
						if (!doing_idle && chart_data.chart_tag == "PCT_BUSY_BY_CPU") {
							let cs_ret = get_cs_str(i, nm);
							build_flame(event_list[fe_2].event, unit, cs_ret.arr, period, hvr_clr, cs_clr, null);
							if (has_cpi && (
								event_list[fe_2].event == cpi_str.cycles ||
								event_list[fe_2].event == cpi_str.time ||
								event_list[fe_2].event == cpi_str.instructions)
								) {
								if (event_list[fe_2].event == cpi_str.cycles) {
									cpi_str.cycles_unit = unit;
								} else if (event_list[fe_2].event == cpi_str.time) {
									cpi_str.time_unit = unit;
								} else if (event_list[fe_2].event == cpi_str.instructions) {
									cpi_str.instructions_unit = unit;
								}
								build_flame("CPI", unit, cs_ret.arr, period, hvr_clr, cs_clr, event_list[fe_2].event);
							}
							if (has_gips && (
								event_list[fe_2].event == cpi_str.time ||
								event_list[fe_2].event == cpi_str.instructions)
								) {
								if (event_list[fe_2].event == cpi_str.time) {
									cpi_str.time_unit = unit;
								} else if (event_list[fe_2].event == cpi_str.instructions) {
									cpi_str.instructions_unit = unit;
								}
								build_flame("GIPS", unit, cs_ret.arr, period, hvr_clr, cs_clr, event_list[fe_2].event);
							}
						}
						if ((chart_data.chart_tag == "WAIT_TIME_BY_proc")) {
							let cs_ret = get_cs_str(i, nm);
							build_flame(context_switch_event+"_offcpu", unit, cs_ret.arr, period, hvr_clr, clr, null);
						}
						if ((chart_data.chart_tag == "RUN_QUEUE" || chart_data.chart_tag == "RUN_QUEUE_BY_CPU")) {
							let cs_ret = get_cs_str(i, nm);
							build_flame(context_switch_event+"_runqueue", unit, cs_ret.arr, period, hvr_clr, clr, null);
						}
						cs_sum += operiod;
						build_fl_tm += performance.now() - tm_fl_here_0;
					}
					let tm_1 = performance.now();
					fl_tm_bld += tm_1 - tm_0;
				}
				if (ch_type == "stacked") {
					let fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
						//fe_idx = chart_data.myshapes[i].ival[IVAL_FE];
					let do_event_highlight = false;
					if (fe_idx == -1 || (typeof event_select[fe_idx] != 'undefined' &&
						(event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show'))) {
						if (fe_idx != -1 && event_select[fe_idx][1] == 'highlight') {
							do_event_highlight = true;
							//console.log("stk hghlight");
						}
					}
					if (do_proc||do_event) {
						// the line_done array is 1 if we've already drawn a line at this pixel. Don't need to overwrite it.
						let wd = end[0] - beg[0];
						let hi = end[1] - beg[1];
						if (wd == 0) {
							wd++;
							beg[0]--;
						}
						let use_clr = clr;
						let cpu= chart_data.myshapes[i].ival[IVAL_CPU];
						let cpt= chart_data.myshapes[i].ival[IVAL_CPT];
						ctx.beginPath();
						ctx.fillStyle = clr;
						ctx.fillRect(beg[0], beg[1], wd, hi);
						if (do_event_highlight) {
							ctx.strokeStyle = 'black';
							ctx.lineWidth = 2;
							ctx.strokeRect(beg[0], beg[1], wd, hi);
							ctx.lineWidth = 1;
						}
						if (1==20) {
						ctx.beginPath();
						ctx.strokeStyle = 'black';
						ctx.lineWidth = 1;
						ctx.moveTo(beg[0], beg[1]);
						ctx.lineTo(end[0], end[1]);
						ctx.stroke();
						}
						line_done[subcat_idx][beg[0]] = 1;
						drew_this_shape = true;
					}
				} else if (ch_type == "line" || line_done[subcat_idx][beg[0]] != 1) {
					let do_event = false;
					let fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
					if (ch_type == "block") {
						fe_idx = chart_data.myshapes[i].ival[IVAL_FE];
					}
					if (fe_idx == -1 || (typeof event_select[fe_idx] != 'undefined' &&
						(event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show'))) {
						do_event = true;
						if (fe_idx != -1 && event_select[fe_idx][1] == 'highlight') {
							do_event_highlight = true;
						}
					}
					if ((ch_type != "line" && do_proc) || do_event) {
						// the line_done array is 1 if we've already drawn a line at this pixel. Don't need to overwrite it.
						ctx.beginPath();
						ctx.strokeStyle = clr;
						if  (ch_type == "line") {
							ctx.lineWidth = 1;
						}
						let do_step = g_do_step;
						if (ch_type != "line") {
							do_step = false;
						}
						let connect_lines = true;
						if (typeof chart_data.marker_connect != undefined && !g_do_step_changed) {
							if (chart_data.marker_connect == "y") {
								do_step = true;
							}
							if (chart_data.marker_connect == "n") {
								do_step = false;
								connect_lines = false;
							}
						}
						let draw_marker = false;
						if (typeof chart_data.marker_type != undefined && typeof chart_data.marker_size != undefined) {
							if (chart_data.marker_type == "square") {
								draw_marker = true;
							}
						}
						let yPxlzero = canvas_px_high(null) - yPadding;
						if (connect_lines) {
							if (do_step) {
								if (do_event_highlight) {
									ctx.lineWidth = 5;
								}
								if (step[subcat_idx][0] > -1 && step[subcat_idx][0] == beg[0]) {
									// case where next value begins where prev value ends
									ctx.moveTo(step[subcat_idx][0], step[subcat_idx][1]);
									ctx.lineTo(beg[0], beg[1]);
								} else if (step[subcat_idx][0] > -1) {
									ctx.moveTo(step[subcat_idx][0], step[subcat_idx][1]);
									ctx.lineTo(step[subcat_idx][0], yPxlzero);
									//ctx.stroke();
									// don't just overdraw lines... slows things down and no extra info
									// so don't draw 2nd part of line up vertical unless it is visibly diff from down vertical
									// As we zoom in then the lines will get drawn anyway (since separation btwn up/down vert will be > 2)
									if (Math.abs(beg[0] - step[subcat_idx][0]) >= 0) {
										ctx.lineTo(beg[0], yPxlzero);
										lkup[subcat_idx].push([step[subcat_idx][0], yPxlzero, beg[0], yPxlzero, i, 0, step[subcat_idx][2], x0]);
										ctx.lineTo(beg[0], beg[1]);
										//ctx.stroke();
									} else {
										ctx.moveTo(beg[0], beg[1]);
									}
								} else {
									ctx.moveTo(beg[0], beg[1]);
								}
							} else {
								ctx.moveTo(beg[0], beg[1]);
							}
							ctx.lineTo(end[0], end[1]);
							ctx.stroke();
							drew_this_shape = true;
						}
						step[subcat_idx][0] = end[0];
						step[subcat_idx][1] = end[1];
						step[subcat_idx][2] = x1;
						if (do_event_highlight) {
							ctx.beginPath();
							ctx.moveTo(beg[0], end[1]);
							if  (ch_type == "line") {
								ctx.lineTo(end[0], end[1]);
								ctx.strokeStyle = clr;
								ctx.lineWidth = 5;
							} else {
								//ctx.lineWidth = 20;
								//ctx.strokeStyle = 'red';
								ctx.strokeStyle = 'black';
								ctx.lineTo(end[0], end[1]+5);
							}
							ctx.stroke();
						}
						drew_lns++;
						ctx.lineWidth = 1;
						line_done[subcat_idx][beg[0]] = 1;
						if (draw_marker && chart_data.marker_type == "square") {
							let sz = +chart_data.marker_size;
							let rct = draw_rect(ctx, end[0], end[1], sz, sz, clr, yPxlzero, do_event_highlight);
							lkup[subcat_idx].push([rct.x0, rct.y0, rct.x1, rct.y1, i]);
							//lkup[subcat_idx].push([end[0]-sz/2, end[1]-sz/2, end[0]+sz/2, end[1]+sz/2, i]);
						}
					}
				} else {
					skipped++;
				}
			}
			if (!filtering || drew_this_shape)
			{
				lkup[subcat_idx].push([beg[0], beg[1], end[0], end[1], i]);
			}
		}
		// now draw __total__ line if needed
		if (tot_line.evt_str.length > 0) {
			for (let sci= 0; sci < tot_line.subcat_rng_arr.length; sci++) {
			//let sc_idx = tot_line.subcat_rng_idx;
			let sc_idx = tot_line.subcat_rng_arr[sci];
			//chart_data.subcat_rng[sc_idx].{x0:mnx, x1:mxx, y0:mny, y1:mxy, fe_idx:mx_fe_idx, event:tot_line.evt_str, total:0.001,
			//	cat:mx_cat, subcat:0, cat_text:tot_line.evt_str});
			let fe_idx = event_list[tot_line.event_list_idx[sci]].idx;
			//let fe_idx = event_list[tot_line.event_list_idx[sci]].fe_idx;
			//console.log("tot_line: sc_idx= "+sc_idx+", yarr= "+tot_line.yarray.length+", ch_title="+chart_data.title+", fe_idx= "+fe_idx);
			let cat = chart_data.subcat_rng[sc_idx].cat;
			let subcat = chart_data.subcat_rng[sc_idx].subcat;
			let subcat_idx = subcat_cs_2_sidx_hash[cat][subcat];
			let clr = event_list[tot_line.event_list_idx[sci]].color;
			//console.log("tot_line: sc_idx= "+sc_idx+", fe_idx= "+fe_idx+", fe_rnk= "+fe_rnk+", clr= "+clr+", subcat_idx= "+subcat_idx);
			let do_event = false;
			let do_event_highlight = false;
			//console.log("ev_lst_idx= "+tot_line.event_list_idx[sci]+", fe_idx= "+fe_idx);
			if (fe_idx != -1 && typeof event_select[fe_idx] != 'undefined') {
				if (event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show') {
					do_event = true;
					if (event_select[fe_idx][1] == 'highlight') {
						do_event_highlight = true;
						console.log(sprintf("__tot_line.highlight fe_idx= %d, sci= %d, tl.eli= %d",
							fe_idx, sci, tot_line.event_list_idx[sci]));
					}
				}
			}
			ctx.beginPath();
			ctx.strokeStyle = clr;
			ctx.lineWidth = 2;
			if (do_event_highlight) {
				ctx.lineWidth = 5;
			}
			let pts_max = tot_line.yarray[sci].length-1;
			let yPxlzero = canvas_px_high(null) - yPadding;
			if (do_event) {
				for (let i=0; i < pts_max; i++) {
					let x0 = tot_line.xarray[i];
					let x1 = tot_line.xarray[i+1];
					let y0 = tot_line.yarray[sci][i];
					let y1 = tot_line.yarray[sci][i];
					let beg = xlate(ctx, x0, y0, minx, maxx, uminy, umaxy);
					let end = xlate(ctx, x1, y1, minx, maxx, uminy, umaxy);
					if (beg[1] > yPxlzero) {
						beg[1] = yPxlzero;
					}
					if (end[1] > yPxlzero) {
						end[1] = yPxlzero;
					}
					ctx.moveTo(beg[0], beg[1]);
					ctx.lineTo(end[0], end[1]);
					lkup[subcat_idx].push([beg[0], beg[1], end[0], end[1], i]);
					if ((i+1) < pts_max) {
						x1 = tot_line.xarray[i+1];
						y1 = tot_line.yarray[sci][i+1];
						end = xlate(ctx, x1, y1, minx, maxx, uminy, umaxy);
						if (end[1] > yPxlzero) {
							end[1] = yPxlzero;
						}
						ctx.lineTo(end[0], end[1]);
					}
				}
				ctx.stroke();
			}
			}
		}
		ctx.lineWidth = 1;
		lkup_use_linearSearch = [];
		for (let ii=0; ii < lkup.length; ii++) {
			if (lkup[ii].length > 0) {
				lkup[ii].sort(sortFunction);
			}
			lkup_use_linearSearch.push(false);
			for (let jj=1; jj < lkup[ii].length; jj++) {
				if (lkup[ii][jj-1][2] <=  lkup[ii][jj][0] &&
					lkup[ii][jj][0] != lkup[ii][jj][2]) {
					continue;
				}
				lkup_use_linearSearch[ii] = true;
				//console.log(sprintf("__lkup_lnrsrc[%d]= %s for %s", ii, lkup_use_linearSearch[ii], chart_data.title));
				break;
			}
		}

		let tm_here_04b = performance.now();
		//console.log("chart_redraw took "+flt_dec(tm_here_04b - tm_here_04a, 4)+" ms");
		//console.log("cs_str_period= "+ (1.0e-9*cs_str_period));
		if ((ch_type == "block" && chart_data.chart_tag == "PCT_BUSY_BY_CPU") ||
			chart_data.chart_tag == "WAIT_TIME_BY_proc" ||
			chart_data.chart_tag == "RUN_QUEUE" ||
			chart_data.chart_tag == "RUN_QUEUE_BY_CPU") {
			let input_evt = "";
			if (chart_data.chart_tag == "WAIT_TIME_BY_proc") {
				input_evt = context_switch_event+"_offcpu";
			}
			if (chart_data.chart_tag == "RUN_QUEUE") {
				input_evt = context_switch_event+"_runqueue";
			}
			if (chart_data.chart_tag == "RUN_QUEUE_BY_CPU") {
				input_evt = context_switch_event+"_runqueue_by_cpu";
			}
			//let tm_here = performance.now();
			//console.log("build_flame_rpt: before:");
			if (1==20 && build_flame_rpt_timeout != null) {
				clearTimeout(build_flame_rpt_timeout);
				build_flame_rpt_timeout = null;
			}
			let sum = 0;
			let bfr_args = {ele:hvr_clr, evt:input_evt};
			//build_flame_rpt_timeout = setTimeout(do_build_flame_rpt, 300, bfr_args);
			//build_flame_rpt_timeout = setTimeout(do_build_flame_rpt, 300, hvr_clr, input_evt);
			let tm_0 = performance.now();
			do_build_flame_rpt(hvr_clr, input_evt);
			let tm_1 = performance.now();
			fl_tm_rpt += tm_1 - tm_0;
			//sum = build_flame_rpt(hvr_clr, input_evt);
			//let tm_now  = performance.now();
			//console.log("build_flame_rpt: after: took msecs= "+(tm_now-tm_here)+", ret sum= "+(1.0e-9 * sum)+", exp sum= "+(1.e-9 * cs_sum));
			//console.log("build_flame_rpt: after: took msecs= "+(tm_now-tm_here), "bld_fl msecs="+build_fl_tm);
			if (build_fl_tm > 500) {
				console.log("build_flame_rpt: bld_fl msecs="+build_fl_tm);
			}
		}
		let tm_now = performance.now();
		let tm_dff = tm_now - tm_here_04a;
		let fl_tm = fl_tm_bld + fl_tm_rpt;
		if (tm_dff > 500 || fl_tm > 500) {
			console.log("__ch["+chrt_idx+"].tm(ms)= "+tm_dff.toFixed(2)+" from "+from_where+", ttl= "+chart_data.title+", fl_tm_bld= "+fl_tm_bld.toFixed(2)+", fl_tm_rpt= "+fl_tm_rpt.toFixed(2)
					);
		}
		if (gsync_zoom_redrawn_charts_map[chrt_idx] == 0) {
			gsync_zoom_redrawn_charts_map[chrt_idx] = 1;
			gsync_zoom_redrawn_charts.cntr++;
		}
	}
	function do_build_flame_rpt(ele, evt)
	{
		//clearTimeout(build_flame_rpt_timeout);
		//build_flame_rpt_timeout = null;
		let tm_here = performance.now();
		//build_flame_rpt_timeout = setTimeout(do_build_flame_rpt, 500, bfr_args);
		//build_flame_rpt(args.ele, args.evt);
		//console.log("do_build_flame_rpt: ele= "+ele+", evt= "+evt);
		build_flame_rpt(ele, evt);
		let tm_now  = performance.now();
		let tm_dff = tm_now-tm_here;
		if (tm_dff > 500.0) {
			console.log("do_build_flame_rpt: after: took msecs= "+tm_dff);
		}
	}
	chart_redraw("main_line_"+chrt_idx);
	let tm_here_05 = performance.now();
	// ctx work end

	mycanvas2_ctx = mycanvas2.getContext('2d');

	function draw_mini(zero_to_one, arg2)
	{
		if (isNaN(zero_to_one)) {
			return;
		}
		//console.log("draw_mini["+chrt_idx+"]("+zero_to_one+", arg= "+arg2+")");
		if ( typeof draw_mini.x_prev == 'undefined' ||
			draw_mini.invocation_num != can_shape.invocation_num) {
			draw_mini.x_prev = -1;
			if (typeof draw_mini.invocation_num == 'undefined') {
			draw_mini.image_rdy = 0;
			draw_mini.image = new Image();
			draw_mini.image.src = mycanvas.toDataURL("image/png");
			//draw_mini.image.src = mycanvas.toDataURL();
			draw_mini.image.onload = function(){
				draw_mini.image_rdy = 1;
				mycanvas2_ctx.drawImage(draw_mini.image, 0, 0, mycanvas2.width, mycanvas2.height);
				//console.log("__draw_mini saved image");
				draw_mini.x_prev = -1;
	//console.log("inside0: hvr_clr= "+hvr_clr+", minx= "+zoom_x0+", maxx= "+zoom_x1+", zoom_y0= "+zoom_y0+", zoom_y1= "+zoom_y1+", chart_data.x_range.max= "+chart_data.x_range.max);
	let win_sz = get_win_wide_high();
				let xn = zoom_x0 + 0.5 * (zoom_x1 - zoom_x0);
				let xd = chart_data.x_range.max - chart_data.x_range.min;
				let xx = 0.5;
				if ( xd > 0.0) {
					xx = xn/xd;
				}
				//console.log(sprintf("chrt= %d, xn= %f, xd= %d, xx= %f", chrt_idx, xn, xd, xx));

				draw_mini(xx, "b");
			};
			}
			draw_mini.invocation_num = can_shape.invocation_num;
			//console.log("__draw_mini save image");
		}

		let x_int = Math.trunc(zero_to_one * 1000);
		if (x_int == draw_mini.x_prev) {
			//console.log("__draw_mini return");
			return;
		}
		//console.log("draw_mini.x_prev= "+draw_mini.x_prev+", x_int= "+x_int+", z21= "+zero_to_one);
		draw_mini.x_prev = x_int;
		mycanvas2_ctx.clearRect(0, 0, mycanvas2.width, mycanvas2.height);
		mycanvas2_ctx.drawImage(draw_mini.image, 0, 0, mycanvas2.width, mycanvas2.height);
		mycanvas2_ctx.fillStyle = 'rgba(0, 204, 0, 0.5)';
		let xbeg = xPadding;
		let xwid = mycanvas2.width - xPadding;
		/*
		console.log("minx= "+minx+", maxx= "+maxx+
				", chart_data.x_range.max= "+chart_data.x_range.max+
				", chart_data.x_range.min= "+chart_data.x_range.min);
		*/
		if (maxx == chart_data.x_range.max && minx == chart_data.x_range.min){
			zero_to_one = 0.5;
		}
		let xmid = zero_to_one * xwid;
		let wd = 16;
		let x0 = xbeg + xmid  - 0.5*wd;
		let x1 = x0 + 0.5*wd;
		let y0 = 0;
		let y1 = mycanvas2.height;
		let rx0 = (x0 - xPadding)/xwid;
		let rx1 = (x1 - xPadding)/xwid;
		mycanvas2_ctx.fillRect(x0, y0, wd, y1);
		mycanvas2_ctx.strokeStyle = 'black';
		mycanvas2_ctx.strokeRect(x0, y0, wd, y1);
		mycanvas2_ctx.stroke();
		draw_mini_box = {x0:x0, x1:x1, y0:y0, y1:y1, rx0:rx0, rx1:rx1};
		let clp_beg = chart_data.ts_initial.tm_beg_offset_due_to_clip;
		let xdiff = +0.5 * (maxx - minx);
		let xn = clp_beg + zero_to_one * (chart_data.x_range.max - clp_beg);
		x0 = +xn - xdiff;
		x1 = +xn + xdiff;
		//console.log("__xn= "+xn+", z2one= "+zero_to_one+", xdff= "+xdiff+", x0= "+x0+", x1= "+x1);
		zoom_to_new_xrange(x0, x1, true);
	}
	if (chart_did_image[chrt_idx] == null || copy_canvas == true) {
		//grab the context from your destination canvas
		chart_did_image[chrt_idx] = mycanvas;
		mycanvas2.onmousemove = function(e) {
			// important: correct mouse position:
			let rect = this.getBoundingClientRect(),
				x = Math.trunc(e.clientX - rect.left),
				y = Math.trunc(e.clientY - rect.top);
			let xn = (x - xPadding)/(mycanvas2.width - xPadding);
			if (xn > 0 && ((draw_mini_box.rx0 - 0.01 ) <= xn && xn <= (draw_mini_box.rx1+0.02))) {
				//console.log("rx0= "+(draw_mini_box.rx0-0.01)+", xn= "+xn+",rx1= "+(draw_mini_box.rx1+0.02));
				gsync_zoom_active_redraw_beg_tm = performance.now();
				gsync_zoom_active_now = true;
				draw_mini_cursor_prev = mycanvas2.style.cursor;
				mycanvas2.style.cursor = 'pointer';
				draw_mini(xn, "c");
			} else {
				//console.log("zoom inactive");
			}
		}
		mycanvas2.onmouseout = function(e) {
			if (draw_mini_cursor_prev != null) {
				mycanvas2.style.cursor = draw_mini_cursor_prev;
				draw_mini_cursor_prev = null;
			}
			gsync_zoom_charts_redrawn = 0;
		    gsync_zoom_charts_hash = {};
			gsync_zoom_active_now = false;
			console.log("zoom inactive");
		}
		draw_mini(0.5, "d");
	}
	for (let i=0; i < lkup.length; i++) {
		lkup[i].sort(sortFunction);
	}

	function set_chart_text(ele_txt,clr_button, txt_str)
	{
		ele_txt.innerHTML = txt_str;
		//let clr_button = document.getElementById(btn_ele_nm);
	    clr_button.style.visibility = 'visible';
	}

	function sortRow(a, b) {
		return a[0]-b[0];
	}
	function sortFunction(a, b) {
	    if (a[0] === b[0]) {
			//if (a[2] === b[2]) {
			//	return a[1] - b[1];
			//}
			return a[2] - b[2];
	    } else {
			return a[0] - b[0];
	    }
	}
	function linearSearch(ar, elx, ely, compare_fn) {
		for (let k=0; k < ar.length; k++) {
			let cmp = compare_fn(elx, ely, ar[k]);
			if (cmp == 0) {
				return k; 
			}
		}
		return -1;
	}
	function binarySearch(ar, elx, ely, compare_fn) {
	    let m = 0;
	    let n = ar.length - 1;
	    while (m <= n) {
			let k = (n + m) >> 1;
			let cmp = compare_fn(elx, ely, ar[k]);
			if (cmp > 0) {
				m = k + 1;
			} else if(cmp < 0) {
				n = k - 1;
			} else {
				return k;
			}
	    }
	    return -m - 1;
	}

	function compare_on_line(x, y, b) {
		if ((x-1)  < b[2]) {
			return -1;
		}
		if ((x+1)  > b[2]) {
			return 1;
		}
		return 0;
		//return a - b;
	}
	function compare_in_rect(x, y, b) {
		if (x  < b[0]) {
			return -1;
		}
		if (x  > b[2]) {
			return 1;
		}
		return 0;
		//return a - b;
	}
	function compare_in_box(x, y, b) {
		if (x  < (b[0]-2)) {
			return -1;
		}
		if (x  > (b[2]+2)) {
			return 1;
		}
		if (y  < (b[1]-2)) {
			return 1;
		}
		if (y  > (b[3]+2)) {
			return -1;
		}
		return 0;
		//return a - b;
	}
	function compare_in_stack(x, y, b) {
		if (x  < b[0]) {
			return -1;
		}
		if (x  > b[2]) {
			return 1;
		}
		if (y  < (b[3])) {
			return 1;
		}
		if (y  > (b[1])) {
			return -1;
		}
		return 0;
		//return a - b;
	}
	var ms_dn_pos = [];

	function get_cs_str(idx, comm) {
		let txt = "";
		let arr = [];
		if (typeof chart_data.myshapes[idx].cs_strs == 'undefined') {
			return {txt: txt, arr: arr};
		}
		let cs_sz = chart_data.myshapes[idx].cs_strs.length;
		for (let i=0; i < cs_sz; i++) {
			let midx = chart_data.myshapes[idx].cs_strs[i];
			gjson_str_pool.str_pool[0].strs[midx] = gjson_str_pool.str_pool[0].strs[midx].replace("[kernel.kallsyms]", "[krnl]");
			let str = gjson_str_pool.str_pool[0].strs[midx];
			txt += "<br>" + str;
			arr.push(str);
		}
		if (typeof comm != 'undefined' && comm != null && comm != "") {
			arr.push(comm);
		}
		txt += "<br>all"; // this is for the last (lowest) 'all' layer of the flamegraph
		arr.push("all");
		return {txt: txt, arr: arr.reverse()};
	}

	let nx0_prev = -1;
	mycanvas.onmousedown = function (e) {
		let rect = this.getBoundingClientRect(),
			x = Math.trunc(e.clientX - rect.left - xPadding),
			y = Math.trunc(e.clientY - rect.top);
	   	ms_dn_pos = [x, y];
	   	mycanvas.onmouseup = function (evt) {
			let rect = this.getBoundingClientRect(),
				x = Math.trunc(evt.clientX - rect.left - xPadding),
				y = Math.trunc(evt.clientY - rect.top);
			let x0 = 1.0 * ms_dn_pos[0];
			let x1 = 1.0 * x;
			let xdiff0= +x0 / (px_wide - xPadding);
			let xdiff1= +x1 / (px_wide - xPadding);
			let nx0 = +minx+( xdiff0 * (maxx - minx));
			//console.log("x0= "+x0+", rec.right= "+rect.right+", maxx= "+maxx+", minx= "+minx+", xdiff0= "+xdiff0+", nx0= "+nx0);
			let nx1 = +minx+(xdiff1 * (maxx - minx));
			console.log("xdiff= "+(x - ms_dn_pos[0])+", nx0= "+nx0+", nx1= "+nx1);
			if (Math.abs(x - ms_dn_pos[0]) <= 3) {
				console.log("Click_1 "+", btn= "+evt.button);
				let str2 = "";
				if (nx0_prev != -1) {
					let diff2 = nx0 - nx0_prev;
					let tm_str2 = tm_diff_str(diff2, 6, 'secs');
					str2 = ", prev_click time= "+ nx0_prev + ", diff= "+ tm_str2;
				}
				let str3 = "";
				if (current_tooltip_text != "") {
					str3 = "<br>" + current_tooltip_text;
				}
				let shape_idx = current_tooltip_shape;
				let str4 = "";
				if (shape_idx > -1) {
					let intrvl_x0 = chart_data.myshapes[shape_idx].pts[PTS_X0]+chart_data.ts_initial.ts - chart_data.ts_initial.ts0x;
					let intrvl_x1 = chart_data.myshapes[shape_idx].pts[PTS_X1]+chart_data.ts_initial.ts - chart_data.ts_initial.ts0x;
					str4 = "<br>abs.T= "+intrvl_x0+ " - " + intrvl_x1;
				}
				let rel_nx0 = nx0 + chart_data.ts_initial.tm_beg_offset_due_to_clip;
				set_chart_text(myhvr_clr_txt, myhvr_clr_txt_btn, "x= " + x + ", rel.T= "+rel_nx0+", T= "+(nx0 + chart_data.ts_initial.ts - chart_data.ts_initial.ts0x)+str4+str2+str3);
				nx0_prev = nx0;
			} else {
				let idx = chrt_idx;
				if (nx1 > nx0) {
					gcanvas_args[idx][6] = nx0;
					gcanvas_args[idx][7] = nx1;
				} else {
					let x0 = minx;
					let x1 = maxx;
					let xfctr = (x1 - x0) / (nx0 - nx1);
					let xdiff = 0.5 * xfctr * (x1 - x0);
					x0 = x0 - xdiff;
					x1 = x1 + xdiff;
					console.log("minx= "+minx+", maxx= "+maxx+", x0= "+x0+", x1="+x1+", xfctr= "+xfctr+", xdiff= "+xdiff);
					if (x0 < chart_data.x_range.min) {
						x0 = chart_data.x_range.min;
					}
					if (x1 > chart_data.x_range.max) {
						x1 = chart_data.x_range.max;
					}
					gcanvas_args[idx][6] = x0;
					gcanvas_args[idx][7] = x1;
				}
				let args = gcanvas_args[idx];
				set_gsync_zoom(gcanvas_args[idx][6], gcanvas_args[idx][7], chart_data.file_tag, chart_data.ts_initial.ts);
				console.log("Drag xdiff= "+(x - ms_dn_pos[0])+", rel pxl from left= "+xdiff1)
				reset_minx_maxx(gcanvas_args[idx][6], gcanvas_args[idx][7], gcanvas_args[idx][8], gcanvas_args[idx][9]);
				chart_redraw("cnvs_mouseup");
			}
			//mycanvas.offmouseup = null;
		};
	};

	function set_gsync_zoom(x0, x1, file_tag, ts_initial_ts) {
		return; // not going to use this now
		let use_idx = -1;
		for (let i=0; i < gsync_zoom_arr.length; i++) {
			if (file_tag == gsync_zoom_arr[i].file_tag) {
				use_idx = i;
				break;
			}
		}
		if (use_idx == -1) {
			gsync_zoom_arr.push({id:-1, x0:-1, x1:-1, file_tag:file_tag});
			for (let i=0; i < gsync_zoom_arr.length; i++) {
				if (file_tag == gsync_zoom_arr[i].file_tag) {
					use_idx = i;
					break;
				}
			}
		}
		if (use_idx > -1) {
			gsync_zoom_arr[use_idx].id++;
			gsync_zoom_arr[use_idx].x0 = x0+ts_initial_ts;
			gsync_zoom_arr[use_idx].x1 = x1+ts_initial_ts;
			gsync_zoom_arr[use_idx].file_tag = file_tag;
		}
	}

	function zoom_to_new_xrange(x0, x1, update_gsync_zoom) {
		if (typeof zoom_to_new_xrange.prev == 'undefined' ||
			zoom_to_new_xrange.invocation_num != can_shape.invocation_num) {
			zoom_to_new_xrange.prev = {};
			zoom_to_new_xrange.invocation_num = can_shape.invocation_num;
		}
		let idx = chrt_idx;
    		//console.log("pan x0= "+x0+", x1= "+x1);
		let xdiff = +0.5 * (maxx - minx);
		/*
		if (x0 > maxx) {
			let xd = x1 - x0;
			x0 = maxx;
			x1 = maxx + xd;
		} else if (x1 < minx) {
			let xd = x1 - x0;
			x1 = minx;
			x0 = minx - xd;
		}
		*/
		//console.log("for 01, x1= "+x1);
		if (update_gsync_zoom) {
			//console.log("zm2nw1: "+x0+",x1="+x1+",ttl="+chart_data.title);
			if (x0 < chart_data.x_range.min) {
				let xd = chart_data.x_range.min - x0;
				x0 += xd;
				x1 += xd;
				if (x1 > chart_data.x_range.max) {
					x1 = chart_data.x_range.max;
				}
				//console.log("for 02, x1= "+x1);
			}
			if (x1 > chart_data.x_range.max) {
				let xd = x1 - chart_data.x_range.max;
				x1 -= xd;
				x0 -= xd;
				//console.log("for 03, x1= "+x1);
				if (x0 < chart_data.x_range.min) {
					x0 = chart_data.x_range.min;
				}
			}
		} else {
			//console.log("zm2nw2: "+x0+",x1="+x1+",ttl="+chart_data.title);
		}
		if (x1 <= x0) {
			return;
		}
		//console.log("for 04, x1= "+x1);
		if (x0 == minx && x1 == maxx && minx == chart_data.x_range.min && maxx == chart_data.x_range.max) {
			// nothing to do, zoomed out to max;
			return;
		}
		if (update_gsync_zoom) {
			set_gsync_zoom(x0, x1, chart_data.file_tag, chart_data.ts_initial.ts);
		}
		//console.log("zoom: x0: "+x0+",x1= "+x1+", abs x0="+(x0+chart_data.ts_initial.ts)+",ax1="+(x1+chart_data.ts_initial.ts));
		gcanvas_args[idx][6] = x0;
		gcanvas_args[idx][7] = x1;
		let args = gcanvas_args[idx];
		reset_minx_maxx(args[6], args[7], args[8], args[9]);
		if (typeof zoom_to_new_xrange.prev.x0 != 'undefined' &&
			zoom_to_new_xrange.prev.x0 == x0 && zoom_to_new_xrange.prev.x1 == x1) {
			//console.log("skip zoom_2_new x0= "+x0+", x1= "+x1);
			return;
		}
		if (typeof zoom_to_new_xrange.prev.x0 == 'undefined') {
			zoom_to_new_xrange.prev = {x0:x0, x1:x1};
		}
		zoom_to_new_xrange.prev = {x0:x0, x1:x1};
		let tm_0 = performance.now();
		//console.log("zoom: x0: "+x0+",x1= "+x1+", abs x0="+(x0+chart_data.ts_initial.ts)+",ax1="+(x1+chart_data.ts_initial.ts));
		chart_redraw("zoom_2_new");
		let tm_1 = performance.now();
		//console.log("ch["+chrt_idx+"].rdrw tm= "+tm_diff_str(tm_1-tm_0, 6, 'secs'));
	}
	function fmt_val(val) {
		let val_str;
		if (chart_data.y_fmt != "") {
			val_str = vsprintf(chart_data.y_fmt, [val]);
		} else {
			val_str = val.toFixed(y_axis_decimals);
		}
		return val_str;
	}
	//console.log("mycanvas.width= "+mycanvas.width);
	mycanvas.onmousemove = function(e) {
		if ( typeof can_shape.hvr_prv_x == 'undefined' ) {
			can_shape.hvr_prv_x == -1;
			can_shape.hvr_prv_y == -1;
		}
		  // important: correct mouse position:
		  let rect = this.getBoundingClientRect(),
			x = Math.trunc(e.clientX - rect.left),
			y = Math.trunc(e.clientY - rect.top);
			//x = (e.clientX - rect.left),
			//y = (e.clientY - rect.top);
			if ((x != can_shape.hvr_prv_x || y != can_shape.hvr_prv_y)) {
				let mtch = -1;
				let rw = -1, row = -1;
				let fnd = -1;
				let fnd_list = [];
				if (ch_type == "line") {
					for (let i = 0; i < lkup.length; i++) {
						if (lkup_use_linearSearch[i]) {
							fnd = linearSearch(lkup[i], x, y, compare_in_box);
						} else {
							fnd = binarySearch(lkup[i], x, y, compare_in_box);
						}
						//let xs = sprintf("row= %d, x= %f, y= %f, fnd= %s, bx= %s", i, x, y, fnd, (fnd < 0 ? "" : lkup[i][fnd]));
						//console.log(xs);
						if (fnd >= 0) {
							row = i;
							if (tot_line.evt_str.length > 0 && typeof tot_line.subcat_rng_idx[row] !== 'undefined') {
								//console.log("got tot_line");
								fnd_list.push({fnd:fnd, row:row});
								//console.log("evt_sel= "+event_select[use_s]+", nm= "+nm+", elk= "+use_e);
							} else {
								let lk = lkup[i][fnd];
								let shape_idx= lk[4];
								let cpt= chart_data.myshapes[shape_idx].ival[IVAL_CPT];
								let use_it = true;
								if (cpt >= 0 && typeof chart_data.proc_arr[cpt] != 'undefined') {
									let nm = chart_data.proc_arr[cpt].comm+" "+chart_data.proc_arr[cpt].pid+"/"+chart_data.proc_arr[cpt].tid;
									let use_e;
									let use_s;
									if (chart_data.chart_tag == "WAIT_TIME_BY_proc" ||
											chart_data.chart_tag == "RUN_QUEUE" ||
											chart_data.chart_tag == "RUN_QUEUE_BY_CPU") {
										use_e = row;
										use_s = row;
									} else {
										use_e = event_lkup[row];
										use_s = event_list[use_e].idx;
									}
									if (typeof use_e == 'undefined') {
										console.log("prob event_lkup["+row+"] not def. i= "+i+", nm= "+nm+", cpt= "+cpt);
										use_it = false;
									} else {
										//use_s = event_list[use_e].idx;
										//console.log("cpt= "+cpt+", evt_sel= "+event_select[use_s]+", nm= "+nm+", elk= "+use_e);
										//console.log(event_list[use_e]);
										if (cpt >= 0 && typeof event_select[use_s] != 'undefined' && event_select[use_s][1] == 'hide') {
											use_it = false;
										}
									}
								}
								if (use_it) {
									fnd_list.push({fnd:fnd, row:row});
								}
							}
						}
					}
				} else if (ch_type == "stacked") {
					for (let i = 0; i < lkup.length; i++) {
						//console.log("stckd i="+i);
						fnd = binarySearch(lkup[i], x, y, compare_in_stack);
						if (fnd < 0) {
							fnd = binarySearch(lkup[i], x-2, y, compare_in_stack);
							if (fnd < 0) {
							fnd = binarySearch(lkup[i], x+2, y, compare_in_stack);
							}
						}
						if (fnd >= 0) {
							//console.log("fnd= "+fnd+",i="+i);
							//console.log(lkup[i][fnd]);
							row = i;
							if (tot_line.evt_str.length > 0 && typeof tot_line.subcat_rng_idx[row] !== 'undefined') {
								//console.log("got tot_line");
								fnd_list.push({fnd:fnd, row:row});
								//console.log("evt_sel= "+event_select[use_s]+", nm= "+nm+", elk= "+use_e);
							} else {
								let lk = lkup[row][fnd];
								let shape_idx= lk[4];
								let cpt= chart_data.myshapes[shape_idx].ival[IVAL_CPT];
								let use_it = true;
								if (cpt >= 0) {
									let nm = chart_data.proc_arr[cpt].comm+" "+chart_data.proc_arr[cpt].pid+"/"+chart_data.proc_arr[cpt].tid;
									let use_e = event_lkup[row];
									let use_s = event_list[use_e].idx;
									//console.log("cpt= "+cpt+", evt_sel= "+event_select[use_s]+", nm= "+nm+", elk= "+use_e);
									//console.log(event_list[use_e]);
									if (cpt >= 0 && typeof event_select[use_s] != 'undefined' && event_select[use_s][1] == 'hide') {
										use_it = false;
									}
								}
								if (use_it) {
									fnd_list.push({fnd:fnd, row:row});
								}
							}
						}
					}
				} else {
					rw = binarySearch(ylkup, y, x, compare_in_rect);
					if (rw < 0) {
						clearToolTipText(mytooltip);
						current_tooltip_text = "";
						current_tooltip_shape = -1;
						return;
					}
					row = ylkup[rw][4];
					fnd = binarySearch(lkup[row], x, y, compare_in_rect);
					if (fnd < 0) {
						fnd = binarySearch(lkup[row], x-1, y, compare_in_rect);
						if (fnd < 0) {
							fnd = binarySearch(lkup[row], x+1, y, compare_in_rect);
						}
					}
				}
				if (fnd >=0 || fnd_list.length > 0) {
					//console.log(lkup[row][fnd]);
					if (fnd_list.length > 0) {
						fnd = fnd_list[0].fnd;
						row = fnd_list[0].row;
					}
					mtch = -2;
					let lk = lkup[row][fnd];
					//console.log("x_= "+x+", y= "+y + ", fnd= "+fnd+", lkup_rect: x0= "+lk[0]+", y0= "+lk[1]+", x1= "+lk[2]+", y1= "+lk[3]);
					mtch = fnd;
					let shape_idx= lk[4];
					let cpt=-1, x0, x1, cpu, val;
					let nm = "unknown1";
					if (tot_line.evt_str.length > 0 && typeof tot_line.subcat_rng_idx[row] !== 'undefined') {
						console.log(sprintf("tot_line row= %s, val= %s", row, tot_line.subcat_rng_idx[row]));
						x0  = tot_line.xarray[shape_idx];
						x1  = tot_line.xarray[shape_idx+1];
						let sci = tot_line.subcat_rng_idx[row];
						val = tot_line.yarray[sci][shape_idx];
						cpu = -1;
					} else {
						x0 = chart_data.myshapes[shape_idx].pts[PTS_X0];
						x1 = chart_data.myshapes[shape_idx].pts[PTS_X1];
						cpu = chart_data.myshapes[shape_idx].ival[IVAL_CPU];
						if (ch_type == "stacked") {
							val = chart_data.myshapes[shape_idx].pts[PTS_Y1] -
								chart_data.myshapes[shape_idx].pts[PTS_Y0];
						} else {
							val = chart_data.myshapes[shape_idx].pts[PTS_Y1];
						}
						cpt= chart_data.myshapes[shape_idx].ival[IVAL_CPT];
						if (cpt >= 0 && typeof chart_data.proc_arr[cpt] != 'undefined' && chart_data.proc_arr[cpt].tid > -1) {
							nm = chart_data.proc_arr[cpt].comm+" "+chart_data.proc_arr[cpt].pid+"/"+chart_data.proc_arr[cpt].tid;
						}
						if (nm == "unknown1") {
							let fe_idx = chart_data.myshapes[shape_idx].ival[IVAL_FE];
							let fe_2 = event_lkup[fe_idx];
							//console.log("got fe_idx= "+fe_2+" flnm_evt.sz= "+event_list.length);
							nm += ", cpt= "+cpt+",shape_idx= "+shape_idx;
							//console.log(chart_data.myshapes[shape_idx]);
						}
					}
					let inserted_value = lk[5];
					if (typeof inserted_value != 'undefined') {
						x0 = lk[6];
						x1 = lk[7];
						val = inserted_value;
					}
					let str = "at T= "+sprintf("%.6f", x0);
					let lines_done=0;
					if (x0 != x1 || ch_type == "stacked") {
						// so we're doing a rectangle
						let tm_dff = tm_diff_str(x1-x0, 6, 'secs');
						str += "-"+sprintf("%.6f", x1)+", dura="+tm_dff+", proc= "+nm+", cpu= "+cpu;
						if (ch_type == "line" || ch_type == "stacked") {
							str += ",<br>"+chart_data.y_by_var+"="+chart_data.subcat_rng[row].cat_text;
							let val_str = fmt_val(val);
							str += ", "+chart_data.y_label+"="+val_str;
							lines_done=0;
							for (let k=1; k < fnd_list.length; k++) {
								let lk2 = lkup[fnd_list[k].row][fnd_list[k].fnd];
								let shape_idx2= lk2[4];
								if (tot_line.evt_str.length > 0 && typeof tot_line.subcat_rng_idx[fnd_list[k].row] !== 'undefined') {
									let myx0  = sprintf("%.6f", tot_line.xarray[shape_idx2]);
									let myx1  = sprintf("%.6f", tot_line.xarray[shape_idx2+1]);
									//let myx1  = tot_line.xarray[shape_idx2+1];
									let sci = tot_line.subcat_rng_idx[fnd_list[k].row];
									let val = tot_line.yarray[sci][shape_idx2];
									str += "<br>x="+myx0+"-"+myx1+","+chart_data.y_by_var+"="+chart_data.subcat_rng[fnd_list[k].row].cat_text;
									let val_str = fmt_val(val);
									str += ", "+chart_data.y_label+"="+val_str;
								} else {
									let myx0 = chart_data.myshapes[shape_idx2].pts[PTS_X0];
									str += "<br>x="+myx0+","+chart_data.y_by_var+"="+chart_data.subcat_rng[fnd_list[k].row].cat_text;
									let val;
									if (ch_type == "stacked") {
										val = chart_data.myshapes[shape_idx2].pts[PTS_Y1] -
											chart_data.myshapes[shape_idx2].pts[PTS_Y0];
									} else {
										val = chart_data.myshapes[shape_idx2].pts[PTS_Y1];
									}
									let val_str = fmt_val(val);
									str += ", "+chart_data.y_label+"="+val_str;
								}
								lines_done++;
								if (lines_done > 5) {
									break;
								}
							}
						}
						if (ch_type == "stacked") {
							let kend = fnd+100;
							if (kend >= lkup[row].length) { kend = lkup[row].length; }
							lines_done=0;
							for (let k=fnd+1; k < kend; k++) {
								let lk2 = lkup[row][k];
								let shape_idx2= lk2[4];
								//let x0t = chart_data.myshapes[shape_idx2].pts[PTS_X0];
								//let x1t = chart_data.myshapes[shape_idx2].pts[PTS_X1];
								let x0t = lk2[0];
								let x1t = lk2[2];
								let nm_prv = nm;
								if (x0t <= x && x <= x1t) {
									let cpt= chart_data.myshapes[shape_idx2].ival[IVAL_CPT];
									if (cpt >= 0 && chart_data.proc_arr[cpt].tid > -1) {
										nm = chart_data.proc_arr[cpt].comm+" "+chart_data.proc_arr[cpt].pid+"/"+chart_data.proc_arr[cpt].tid;
									}
									let myx0 = chart_data.myshapes[shape_idx2].pts[PTS_X0];
									let myy1 = chart_data.myshapes[shape_idx2].pts[PTS_Y1];
									let myy0 = chart_data.myshapes[shape_idx2].pts[PTS_Y0];
									if (nm != "" && nm != nm_prv) {
										str += "<br>x="+myx0+","+chart_data.y_by_var+"="+chart_data.subcat_rng[row].cat_text;
										//str += "<br>x="+myx0+","+chart_data.y_var+"="+(myy1-myy0);
										str += ", "+nm;
										str += ", val= "+(myy1-myy0);
									}
								} else {
									break;
								}
								lines_done++;
								if (lines_done > 5) {
									break;
								}
							}
							let kbeg = fnd-100;
							if (kbeg < 0) { kbeg = 0; }
							lines_done=0;
							for (let k=fnd-1; k > kbeg; k--) {
								let lk2 = lkup[row][k];
								let shape_idx2= lk2[4];
								//let x0t = chart_data.myshapes[shape_idx2].pts[PTS_X0];
								//let x1t = chart_data.myshapes[shape_idx2].pts[PTS_X1];
								let x0t = lk2[0];
								let x1t = lk2[2];
								let nm_prv = nm;
								if (x0t <= x && x <= x1t) {
									let cpt= chart_data.myshapes[shape_idx2].ival[IVAL_CPT];
									if (cpt >= 0 && chart_data.proc_arr[cpt].tid > -1) {
										nm = chart_data.proc_arr[cpt].comm+" "+chart_data.proc_arr[cpt].pid+"/"+chart_data.proc_arr[cpt].tid;
									}
									let myx0 = chart_data.myshapes[shape_idx2].pts[PTS_X0];
									let myy1 = chart_data.myshapes[shape_idx2].pts[PTS_Y1];
									let myy0 = chart_data.myshapes[shape_idx2].pts[PTS_Y0];
									if (nm != "" && nm != nm_prv) {
										str += "<br>x="+myx0+","+chart_data.y_by_var+"="+chart_data.subcat_rng[row].cat_text;
										//str += "<br>x="+myx0+","+chart_data.y_by_var+"="+(myy1-myy0);
										str += ", "+nm+", val= "+(myy1-myy0);
									}
								} else {
									break;
								}
								lines_done++;
								if (lines_done > 5) {
									break;
								}
							}
						}
					} else {
						let fe_idx = chart_data.myshapes[shape_idx].ival[IVAL_FE];
						let ev_str = chart_data.flnm_evt[fe_idx].event;
						str += ", proc= "+nm+", cpu= "+cpu+"<br>evt= "+ev_str;
					}
					current_tooltip_shape = shape_idx;
					current_tooltip_text = str;
					if (typeof chart_data.myshapes[shape_idx] != 'undefined' &&
						typeof chart_data.myshapes[shape_idx].txtidx != 'undefined') {
						let tstr = gjson_str_pool.str_pool[0].strs[chart_data.myshapes[shape_idx].txtidx];
						let n = tstr.indexOf("_P_");
						if (n == 0) {
							let cpt= chart_data.myshapes[shape_idx].ival[IVAL_CPT];
							if (cpt > -1) {
								let cpu_str = sprintf("[%03d] ", cpu);
								let tm_str = sprintf("%.9f: ", (chart_data.ts_initial.ts +chart_data.myshapes[shape_idx].pts[PTS_X1]));
								let fe_idx = chart_data.myshapes[shape_idx].ival[IVAL_FE];
								let ph_str = chart_data.proc_arr[cpt].comm+" "+
									chart_data.proc_arr[cpt].pid+"/"+chart_data.proc_arr[cpt].tid+" "+ 
									cpu_str + tm_str + chart_data.myshapes[shape_idx].ival[IVAL_PERIOD] + " " +
									chart_data.flnm_evt[fe_idx].event;
								tstr = tstr.replace("_P_", ph_str);
							}
						}
						current_tooltip_text += "<br>"+tstr;
					}
					if (tot_line.evt_str.length == 0) {
						let cs_ret = get_cs_str(shape_idx, nm);
						current_tooltip_text += cs_ret.txt;
					}
    					mytooltip.style.borderWidth = '1px';
    					mytooltip.innerHTML = current_tooltip_text;
    					mytooltip.setAttribute("visibility", 'visible');
    					mytooltip.setAttribute("display", 'inline-block');
					if (x > (0.5 * mycanvas.width)) {
						mytooltip.style.removeProperty('left');
						mytooltip.style.right = (mycanvas.width - x + 20) + 'px';
					} else {
						mytooltip.style.removeProperty('right');
						mytooltip.style.left = (x + 20) + 'px';
					}
					mytooltip.style.removeProperty('bottom');
					mytooltip.style.top = (y + 20) + 'px';
					let visible = checkVisible(mytooltip);
					if (!visible) {
						//tooltip_top_bot = 'top';
						mytooltip.style.removeProperty('top');
						mytooltip.style.bottom = (mycanvas.height - y - 20) + 'px';
					}
					//console.log("tt x= "+x+", y= "+y);
				} else {
					clearToolTipText(mytooltip);
					current_tooltip_text = "";
					current_tooltip_shape = -1;
				}
				//console.log("x_= "+x+", y= "+y + ", mtch= "+mtch+", fnd= "+fnd);
				can_shape.hvr_prv_x = x;
				can_shape.hvr_prv_y = y;
			}
	};
	mycanvas.onmouseout = function(e) {
		clearToolTipText(mytooltip);
		current_tooltip_text = "";
		current_tooltip_shape = -1;
	}
	function flt_dec(num, dec) {
		return num.toFixed(dec);
	}
	let tm_here_06 = performance.now();
	//console.log("cs time01= "+(tm_here_01 - tm_top0).toFixed(2));
	//console.log("cs time02= "+(tm_here_02 - tm_here_01).toFixed(2));
	//console.log("cs time03= "+(tm_here_03 - tm_here_02).toFixed(2));
	//console.log("cs time04= "+(tm_here_04 - tm_here_03).toFixed(2));
	//console.log("cs time05= "+(tm_here_05 - tm_here_04).toFixed(2));
	//console.log("cs time06= "+(tm_here_06 - tm_here_05).toFixed(2));
	let tm_dff = tm_here_06 - tm_top0;
	if (tm_dff > 500) {
		console.log("cs time= "+tm_dff.toFixed(2));
	}
	g_charts_done.cntr++;
	return;
}
/*
}());
*/

function standaloneJob(i, j, sp_data2, ch_data2, tm_beg)
{
	if ( typeof standaloneJob.data == 'undefined' ) {
		standaloneJob.data = null;
		standaloneJob.chdata = [];
	}
  return new Promise(resolve => {
  	console.log('Start standaloneJob: i= ' + i + ", j= "+j);
	let tm_bef = performance.now();
	let wrk_arr = ["decompress string pool", "parse string pool", "decompress chart data", "parse chart data", "gen charts"];
	let wrk = wrk_arr[i];
	mymodal_span_text.innerHTML = "start " + wrk;
	if (i == 0) {
		let st_pool = decompress_str('str_pool', sp_data2);
		standaloneJob.data = st_pool;
	}
	if (i == 1) {
		parse_str_pool(standaloneJob.data);
	}
	if (i == 2 && j > -1 && j < ch_data2.length) {
		standaloneJob.chdata.push(decompress_str('chrt_data', ch_data2[j]));
		//standaloneJob.data = ch_data;
	}
	let ch_len = 0;
	if (i == 3 && j > -1 && j < standaloneJob.chdata.length) {
		if (j == 0) {
			gjson.chart_data = [];
		}
		ch_len = standaloneJob.chdata[j].length;
		console.log("try parse_chart_data j="+j+", sz= "+ch_len);
		parse_chart_data(standaloneJob.chdata[j]);
	}
	if (i == 4) {
		standaloneJob.chdata = [];
		start_charts();
	}
	let tm_now = performance.now();
	//alert("chart "+ chrts_started);
	let elap_tm_tot = tm_diff_str(0.001*(tm_now-g_tm_beg), 2, "secs");
	let elap_tm     = tm_diff_str(0.001*(tm_now-tm_bef), 2, "secs");
	let wrk_nxt = "";
	let n_of_m = "";
	if (i < 4) {
		//wrk_nxt = "<br>starting "+wrk_arr[i+1];
		if (i == 2) {
			wrk_nxt = "<br>";
			let sz_str = sprintf(" chdata["+j+"] is %.3f MB,", [1.0e-6 * (ch_data2[j].length)]);
			wrk_nxt += " (input is "+sz_str+" please be patient...a 10 MB file can take ~20 seconds)"
			n_of_m = ", file "+j+ " of "+ch_data2.length;
		}
		if (i == 3) {
			wrk_nxt = "<br>";
			let sz_str = sprintf(" chdata["+j+"] is %.3f MB,", [1.0e-6 * ch_len]);
			wrk_nxt += " input is "+sz_str;
			n_of_m = ", file "+j+ " of "+ch_data2.length;
		}
	}
	if (i == 5) {
		if (g_cpu_diagram_flds != null) {
			console.log("++++g_charts_done bef= "+g_charts_done.cntr);
			let jj=0;
			function myDelay () {           //  create a loop function
				setTimeout(function () {    //  call a 3s setTimeout when the loop is called
					console.log('jj= '+jj);          //  your code here
					jj++;                     //  increment the counter
					if (g_charts_done.cntr < gjson.chrt_data_sz) {
						myDelay();             //  ..  again which will trigger another 
					} else {
						parse_svg();
						console.log("++++g_charts_done aft= "+g_charts_done.cntr);
						console.log("__did parse_svg()");
					}
				}, 1000)
			}
			//abcd
			if (g_charts_done.cntr < gjson.chrt_data_sz) {
				myDelay();
			}
		}
	}

	mymodal_span_text.innerHTML = "finished work "+wrk+n_of_m+", tm_tot_elap="+elap_tm_tot+", tm_this_chrt= "+elap_tm+wrk_nxt;
	setTimeout(() => {
		//console.log('End: ' + i);
		resolve(i);
	}, g_parse_delay);
  });
}

function doJob(i, grf, chrts_started_max, tm_beg)
{
  return new Promise(resolve => {
  	//console.log('Start: ' + i);
	//gcanvas_args[i] = [ i, chart_divs[i], gjson.chart_data[i], tm_beg, hvr_nm, pxls_high, zoom_x0, zoom_x1, zoom_y0, zoom_y1];
	//can_shape(i, chart_divs[i], gjson.chart_data[i], tm_beg, hvr_nm, pxls_high, zoom_x0, zoom_x1, zoom_y0, zoom_y1);
	let tm_bef = performance.now();
	can_shape(i, gcanvas_args[i][1], gcanvas_args[i][2], gcanvas_args[i][3], gcanvas_args[i][4],
		gcanvas_args[i][5], gcanvas_args[i][6], gcanvas_args[i][7], gcanvas_args[i][8], gcanvas_args[i][9]);
	let tm_now = performance.now();
	//alert("chart "+ chrts_started);
	let elap_tm     = tm_diff_str(0.001*(tm_now-tm_bef), 2, "secs");
	let elap_tm_tot = tm_diff_str(0.001*(tm_now-g_tm_beg), 2, "secs");
	mymodal_span_text.innerHTML = "finished chart "+grf+" of "+chrts_started_max+
	  ", tm_this_chrt= "+elap_tm + ", tot_elap_tm= "+elap_tm_tot;
    setTimeout(() => {
		//console.log('End: ' + i);
		resolve(i);
    }, g_parse_delay);
  });
}

async function start_charts() {
	let tm_beg = performance.now();
	let tm_now = performance.now();
	console.log("can_shape beg. elap ms= " + (tm_now-tm_beg));
	chart_divs.length = 0;
	let ch_did_it = [];
	for (let i=0; i < gjson.chart_data.length; i++) {
		chart_did_image.push(null);
		ch_did_it.push(false);
	}
	let ch_titles = [];
	let got_match;
	//console.log(gjson.categories);
	for (let i=0; i < gjson.chart_data.length; i++) {
		got_match = false;
		ch_titles.push([]);
		for (let j=i+1; j < gjson.chart_data.length; j++) {
			if (gjson.chart_data[i].title == gjson.chart_data[j].title) {
				if (!got_match) {
					ch_titles[i].push(i);
				}
				ch_did_it[j] = true;
				got_match = true;
				ch_titles[i].push(j);
			}
		}
		if (!got_match && !ch_did_it[i]) {
			ch_titles[i].push(i);
		}
	}
	for (let i=0; i < gjson.chart_data.length; i++) {
		//console.log(ch_titles[i]);
		gcanvas_args.push([]);
	}
	gjson.categories.sort(sortByPriority);
	function sortByPriority(a, b) {
		return a.priority - b.priority;
	}
	//console.log(gjson.categories);
	gmsg_span.innerHTML = "hey";
	//update_status("starting graphs");
	let tm_top = performance.now();
	let tm_top0 = tm_top;
	let chrts_started = 0;
	lhs_menu_ch_list = [];
	for (let kk=0; kk < gjson.categories.length; kk++) {
		for (let j=0; j < ch_titles.length; j++) {
			for (let k=0; k < ch_titles[j].length; k++) {
				let i = ch_titles[j][k];
				let cat = gjson.chart_data[i].chart_category;
				if (cat.toLowerCase() != gjson.categories[kk].name.toLowerCase()) {
					continue;
				}
				chrts_started++;
				//update_status("started graph "+chrts_started);
			}
		}
	}
	let chrts_started_max = chrts_started;
	chrts_started = 0;
	//myBarMove(0.0, chrts_started_max);
	if (typeof gjson.pixels_high_default != 'undefined' && gjson.pixels_high_default >= 100) {
		gpixels_high_default = gjson.pixels_high_default;
	}
	if (typeof gjson.phase != 'undefined') {
		console.log(gjson.phase);
	}
	for (let kk=0; kk < gjson.categories.length; kk++) {
		for (let j=0; j < ch_titles.length; j++) {
			for (let k=0; k < ch_titles[j].length; k++) {
				let i = ch_titles[j][k];
				//let this_title = gjson.chart_data[i].title;
				let cat = gjson.chart_data[i].chart_category;
				if (cat.toLowerCase() != gjson.categories[kk].name.toLowerCase()) {
					continue;
				}
				chrts_started++;
				//myBarMove(chrts_started, chrts_started_max);
				//update_status("started graph["+chrts_started+"]");
				tm_now = performance.now();
				//alert("chart "+ chrts_started);
				//document.title = "grf "+chrts_started+"/"+chrts_started_max+",tm="+tm_diff_str(0.001*(tm_now-g_tm_beg), 1, "secs");
				//mymodal_span_text.innerHTML = document.title;
				if ((tm_now - tm_top) > 2000.0) {
					//update_status("started graph "+chrts_started);
					tm_top = tm_now;
				}
				let div_nm = "chrt_div_"+i;
				let hvr_nm = "chrt_hvr_"+i;
				chart_divs.push(div_nm);
				// define the lhs_menu list of charts and the needed info for each chart (div, tags, cat, title, etc)
				lhs_menu_ch_list.push({cat_idx:kk, ch_idx:i, fl_hsh:{}, fl_arr:[], div_nm:div_nm, file_tag_idx:k, file_tag_arr:ch_titles[j], hvr_nm:hvr_nm, title: gjson.chart_data[i].title });
				//console.log("ch_list["+(lhs_menu_ch_list.length-1)+"]= "+ch_titles[j]);
				let zoom_x0, zoom_x1, zoom_y0, zoom_y1;
				zoom_x0 = gjson.chart_data[i].x_range.min;
				zoom_x1 = gjson.chart_data[i].x_range.max;
				zoom_y0 = gjson.chart_data[i].y_range.min;
				zoom_y1 = gjson.chart_data[i].y_range.max;
				let pxls_high = gjson.pixels_hight_default;

				//console.log("zoom_x0= " + zoom_x0 + ", zoom_x1= " + zoom_x1);
				gcanvas_args[i] = [ i, chart_divs[i], gjson.chart_data[i], tm_beg, hvr_nm, pxls_high, zoom_x0, zoom_x1, zoom_y0, zoom_y1];
				//can_shape(i, chart_divs[i], gjson.chart_data[i], tm_beg, hvr_nm, pxls_high, zoom_x0, zoom_x1, zoom_y0, zoom_y1);
				let result = await doJob(i, chrts_started, chrts_started_max, tm_beg);
			}
		}
	}
	document.title = doc_title_def;
	tm_now = performance.now();
	//console.log("tm_now - tm_top0= "+(tm_now - tm_top0));
	update_status("finished "+chrts_started+" graphs, chart loop took "+tm_diff_str(0.001*(tm_now-tm_top0), 3, "secs"));
	mymodal.style.display = "none";
	document.getElementById("chart_container").style.display = "block";
	// if (msWriteProfilerMark) {
	//   msWriteProfilerMark("mark1");
	// }

	let lhs_menu_str = "";
	let cat_idx_prv = -1;
	let lvl = -1;
	let lvl_sub = -1;
	lhs_menu_nm_list = [];
	let dad = -1;
	let cb_cls_str='';
	{
		lhs_menu_str += '<ul class="ul_none" style="line-height:100%">';
		let i = -1;
		let nm  = 'lhs_menu_root-'+0;
		let txt = "show all charts";
		lhs_menu_nm_list.push({menu_i:i, nm:nm, txt:txt, lvl_sub:-1, dad:-1, kids:[]});
		dad = -1;
		lhs_menu_str += '<li class="il_none"><input type="checkbox" name="'+nm+'" id="'+nm+'" onchange="lhs_menu_change(this,'+i+','+i+');" '+cb_cls_str+'><label style="margin-top: 0px;margin-bottom:0px" for="'+nm+'">'+txt+'</label ></li></ul>';
	}
	for (let i=0; i < lhs_menu_ch_list.length; i++) {
		let ch_idx = lhs_menu_ch_list[i].ch_idx;
		let cat_idx = lhs_menu_ch_list[i].cat_idx;
		if (cat_idx_prv != cat_idx) {

			cat_idx_prv = cat_idx;
			lvl++;
			lvl_sub = -1;
			lhs_menu_str += '<ul class="ul_none" style="line-height:100%">';
			let nm  = 'lhs_menu_lvl-'+lvl;
			let txt = gjson.categories[cat_idx].name;
			lhs_menu_nm_list.push({menu_i:i, nm:nm, txt:txt, lvl_sub:lvl_sub, dad:-1, kids:[]});
			dad = lhs_menu_nm_list.length - 1;
			lhs_menu_str += '<li class="il_none"><input type="checkbox" name="'+nm+'" id="'+nm+'" onchange="lhs_menu_change(this,'+i+','+dad+');" '+cb_cls_str+'><label style="margin-top: 0px;margin-bottom:0px" for="'+nm+'">'+txt+'</label ><ul class="ul_none"  style="line-height:100%;font-weight:normal;margin-top: 0px;margin-bottom:0px">';
		}
		lvl_sub++;
		let nm  = 'lhs_menu_lvl-'+lvl+'-'+lvl_sub;
		lhs_menu_ch_list[i].nm = nm;
		let txt = lhs_menu_ch_list[i].title;
		lhs_menu_nm_list.push({menu_i:i, nm:nm, txt:txt, lvl_sub:lvl_sub, dad:dad, kids:[]});
		let nm_idx = lhs_menu_nm_list.length - 1;
		lhs_menu_nm_list[dad].kids.push(nm_idx);

		lhs_menu_str += '<li class="il_none"><input type="checkbox" name="'+nm+'" id="'+nm+'" onchange="lhs_menu_change(this,'+i+','+nm_idx+');" '+cb_cls_str+'><label onmouseover="javascript:menu_hover('+i+');" for="'+nm+'" style="font-weight:normal;margin: 0em 0em 5px 0;margin-top: 0px;margin-bottom:0px">'+txt+'</label></li>';
		if ((i+1) == lhs_menu_ch_list.length || lhs_menu_ch_list[i].cat_idx != lhs_menu_ch_list[i+1].cat_idx) {
			lhs_menu_str += '</ul></li></ul>';
		}
	}
	{
		let i = -2;
		lhs_menu_str += '<hr style="margin-top:0em; margin-bottom:0em;" /><ul class="ul_none" style="line-height:100%">';
		let nm  = 'lhs_menu_root-'+1;
		let txt = "line charts: connect dashes";
		lhs_menu_nm_list.push({menu_i:i, nm:nm, txt:txt, lvl_sub:-1, dad:-1, kids:[]});
		dad = -1;
		lhs_menu_str += '<li class="il_none"><input type="checkbox" name="'+nm+'" id="'+nm+'" onchange="lhs_menu_change(this,'+i+','+i+');" '+cb_cls_str+'><label style="margin-top: 0px;margin-bottom:0px" for="'+nm+'" title="for line charts, the data is sent from the backend like a \'scattered dash\' plot. If this option is checked then the \'dashes\' are connected with vertical lines (if the X end of one dash is less than 3 pixels from the X start of the next dash). If this option is not checked then just the \'dashes\' are drawn. The reason for this option is if there are too many dashes then the chart is just completely filled in with the connecting lines. After you change this option, the chart is not redrawn. The connecting lines will be drawn (or dropped) when you do something like zoom in. This option basically toggles line charts between \'scattered dash\' charts and \'step\' charts.">'+txt+'</label ></li>';
		i = -3;
		nm  = 'lhs_menu_root_fg';
		txt = "Flamegraph: by comm, pid, tid";
		lhs_menu_nm_list.push({menu_i:i, nm:nm, txt:txt, lvl_sub:-1, dad:-1, kids:[]});
		lhs_menu_str += '<li class="il_none"><input type="checkbox" name="'+nm+'" id="'+nm+'" onchange="lhs_menu_change(this,'+i+','+i+');" '+cb_cls_str+'><label style="margin-top: 0px;margin-bottom:0px" for="'+nm+'" id="'+nm+'_label" title="for flamegraph charts, the \'process\' which caused the flamegraph is at the base of the callstack. By default, the base is set to the process name (comm), pid, tid. This can result in more \'stacks\' than you need. Say that each thread is doing the same work. So it would be helpful to categorize by process+pid instead of process+pid+tid. This option lets you cycle through 1) process, pid, tid, 2) process, pid and 3) process. The flamegraphs won\'t be automatically regenerated. You can regenerate them by zomming in or out on the \'cpu busy\' chart.">'+txt+'</label ></li>';
		i = -4;
		nm  = 'lhs_menu_root_linkzoom';
		txt = gsync_text;
		lhs_menu_nm_list.push({menu_i:i, nm:nm, txt:txt, lvl_sub:-1, dad:-1, kids:[]});
		lhs_menu_str += '<li class="il_none"><input type="checkbox" name="'+nm+'" id="'+nm+'" onchange="lhs_menu_change(this,'+i+','+i+');" '+cb_cls_str+'><label style="margin-top: 0px;margin-bottom:0px" for="'+nm+'" id="'+nm+'_label" title="This option changes pan zoom behavior. If the zooming is \'UnLinked\' then zooming/panning on one chart doesn\'t change any other chart. If zooming is \'Linked\' then zooming or panning on one chart also zooms every other chart in the chart group. If you have more than one chart group then the other chart groups aren\'t affected. The absolute time is used for the common X interval. The interval of the last chart zoomed/panned is applied to all the charts in the group. Note that flamegraphs \'zoom\' whenever the \'cpu busy\' chart is zoomed/panned regardless of the zoom link state.">'+txt+'</label ></li>';
		nm  = 'lhs_menu_root_pixels_high';
		txt = "Pixels high";
		lhs_menu_nm_list.push({menu_i:i, nm:nm, txt:txt, lvl_sub:-1, dad:-1, kids:[]});
		lhs_menu_str += '<li class="il_none"><label style="margin-top: 0px;margin-bottom:0px" for="'+nm+'" id="'+nm+'_label" title="This option changes the default pixels high for each chart.">'+txt+'</label><input type="text" name="'+nm+'" id="'+nm+'" maxlength="4" size="4" onchange="change_pixels_high(this,'+i+','+i+');" '+cb_cls_str+'></li>';
		lhs_menu_str += '</ul>';
	}
	let lhs_ele = document.getElementById("lhs_menu_span");
	lhs_ele.innerHTML = lhs_menu_str;
	for (let i=0; i < lhs_menu_nm_list.length; i++) {
		let ele = document.getElementById(lhs_menu_nm_list[i].nm);
		ele.indeterminate = false;
		ele.checked = true;
	}
	let ele = document.getElementById('lhs_menu_root_pixels_high');
	ele.value = gpixels_high_default;


	lhs_menu_ch_list_state = 1; // lhs_menu list finalized
	if (g_got_cpu_diagram_svg) {
		parse_svg();
		console.log("svg len= "+g_svg_obj.str.length);
		webSocket.send("parse_svg="+g_svg_obj.str);
	}
	console.log(sprintf("gjson.chart_data.length %d g_chrts_done= %d", gjson.chart_data.length, g_charts_done.cntr));
	let jj = 0, jjmax = 5;
	function myDelay (lkfor_max, typ) {           //  create a loop function
		setTimeout(function () {    //  call a 3s setTimeout when the loop is called
			console.log('====jj= '+jj+', charts_done= '+g_charts_done.cntr+", redrw= "+gsync_zoom_redrawn_charts.cntr+", lkfor= "+lkfor_max.cntr+", typ= "+typ);
			jj++;                     //  increment the counter
			if (jj < jjmax && ((typ == "all_charts" && lkfor_max.cntr < gjson.chart_data.length) ||
				(typ == "zoom_some" && lkfor_max.cntr < gsync_zoom_redrawn_charts.need_to_redraw))) {
				myDelay(lkfor_max, typ);             //  ..  again which will trigger another 
			} else {
				//parse_svg();
				console.log("++++g_charts_done aft= "+gsync_zoom_redrawn_charts.cntr);
				console.log("__did wait after parse_svg()");
				ck_phase();
			}
		}, 1000, lkfor_max, typ)
	}
	myDelay(g_charts_done, "all_charts");

	console.log("---- did myDelay(g_charts_done)");

	let did_ck_phase = 0;
	function ck_phase()
	{
		if (did_ck_phase == 1) {
			console.log("exit ck_phase");
			gsync_zoom_redrawn_charts.cpu_diag_redraw++;
			return;
		}
		jj =0;
		did_ck_phase = 1;
		if (g_charts_done.cntr == gjson.chart_data.length) {
			g_charts_done.cntr = 0;
		}
		if (typeof gjson.phase != 'undefined') {
			for (let i=0; i < gjson.phase.length; i++) {
				if (gjson.phase[i].zoom_to == 1) {
					gsync_zoom_last_zoom.chrt_idx = -1;
					gsync_zoom_last_zoom.x0 = -1;
					gsync_zoom_last_zoom.x1 = -1;
					gsync_zoom_last_zoom.abs_x1 = gjson.phase[i].ts_abs;
					gsync_zoom_last_zoom.abs_x0 = gjson.phase[i].ts_abs - gjson.phase[i].dura;
					console.log(sprintf("===zoom x0= %s, x1= %s", gsync_zoom_last_zoom.abs_x0, gsync_zoom_last_zoom.abs_x1));
					set_zoom_all_charts(-1, gjson.phase[i].file_tag);
					jjmax=10;
					myDelay(gsync_zoom_redrawn_charts, "zoom_some");
					console.log("---- did myDelay(gsync_zoom_redrawn_charts)");
					break;
				}
			}
		}
	}

	return;
}

function parse_chart_data(ch_data_str)
{
	let str = vsprintf("%.1f MBs", [ch_data_str.length/(1024.0*1024.0)]);
	var tm_beg = performance.now();
	//document.title = "bef JSON "+str+",tm="+tm_diff_str(0.001*(tm_beg-g_tm_beg), 1, "secs");
	gjson.chart_data.push(JSON.parse(ch_data_str));
	let tm_now = performance.now();
	//document.title = "aft JSON "+str+",tm="+tm_diff_str(0.001*(tm_now-g_tm_beg), 1, "secs");
	//console.log("parse chart_data end. elap ms= "+ (tm_now-tm_beg)+", tm_btwn_frst_this_msg= "+(tm_beg - g_tm_beg));
}

function parse_str_pool(str_pool)
{
	let tm_beg = performance.now();
	document.title = "parse str_pool";
	gjson_str_pool = JSON.parse(str_pool);
	document.title = "aft JSON.parse str_pool";
	let str_len = gjson_str_pool.str_pool[0].strs.length;
	let tm_now = performance.now();
	g_tm_beg = tm_now;
}



function openSocket(port_num) {
	// Ensures only one connection is open at a time
	if(webSocket !== undefined && webSocket.readyState !== WebSocket.CLOSED){
		console.log("WebSocket is already opened.");
		return;
	}
	// Create a new instance of the websocket
	webSocket = new WebSocket("ws://localhost:"+port_num);

    webSocket.binaryType = 'arraybuffer';
    /*
     * Binds functions to the listeners for the websocket.
     */
    webSocket.onopen = function(event){
        // for some reason, onopen seems to get called twice
        // and the first time event.data is undefined.
        update_status("websocket opened. ck tab title for status msgs");
		document.title = "Waiting on data";
		mymodal.style.display = "block";
		mymodal_span_text.innerHTML = document.title;
		webSocket.send("Ready");
    };

    webSocket.onmessage = function(event){
        var sln = event.data.length;
		//console.log("msg slen= "+sln+", type= "+ typeof(event.data));
		if(typeof event.data === 'string' ) {
			//console.log('Rec str0-20= '+event.data.substr(0, 20));
			//document.title = "got msg: "+event.data.substr(0,8);
			//mymodal_span_text.innerHTML = "loading data, creating charts. See page tab for status msgs."
		}
		if(event.data instanceof ArrayBuffer ){
			let tm_beg = performance.now();
			let buffer = event.data;
			console.log("got arrayBuffer");
			//var dv = new DataView(buffer);
			let ua = new Uint8Array(buffer);
			let str = String.fromCharCode.apply(null, ua);
			//var vector_length = dv.getUint8(0);
			let tm_now = performance.now();
			console.log("str len= "+str.length+", tm= "+(tm_now-tm_beg));
		}
		ckln = ck_cmd(event.data, sln, 'cpu_diagram=');
		if (ckln > 0) {
			g_got_cpu_diagram_svg = true;
			document.getElementById("cpu_diagram_svg").innerHTML = event.data.substring(ckln);
		}
		ckln = ck_cmd(event.data, sln, 'cpu_diagram_flds=');
		if (ckln > 0) {
			g_cpu_diagram_flds = JSON.parse(event.data.substring(ckln));
			console.log("__got cpu_diagram_flds len= "+g_cpu_diagram_flds.cpu_diagram_fields.length);
		}
		ckln = ck_cmd(event.data, sln, '{"str_pool":');
		if (ckln > 0) {
			parse_str_pool(event.data);
		}
		var ckln = ck_cmd(event.data, sln, "send_int=");
		if (ckln > 0) {
			console.log("websocket.onmessage: "+event.data);
			var str = event.data.substring(ckln);
			console.log("got int= '" + str+"', ckln= " + ckln);
			webSocket.send("int= " + str);
			console.log("client got msg from srvr: "+str);
			if (str == "1") {
				chrt_gen();
			}
			return;
		}
		ckln = ck_cmd(event.data, sln, 'chrts_json_sz= ');
		if (ckln > 0) {
			gjson.chrt_data_sz = parseInt(event.data.substring(ckln));
			//if (typeof gjson.chart_data != 'undefined') {
			//	gjson.chart_data.length = 0;
			//}
			gjson.chart_data = [];
			return;
		}
		ckln = ck_cmd(event.data, sln, 'chrt_cats= ');
		if (ckln > 0) {
			gjson = JSON.parse(event.data.substring(ckln));
			return;
		}
		ckln = ck_cmd(event.data, sln, '{ "title":');
		if (ckln > 0) {
			let tm_beg = performance.now();
			//mymodal_span_text.innerHTML = "start parse_chart_data";
			setTimeout(function(){
				parse_chart_data(event.data);
				let tm_end = performance.now();
				let elap_tm     = tm_diff_str(0.001*(tm_end-tm_beg), 2, "secs");
				let elap_tm_tot = tm_diff_str(0.001*(tm_end-g_tm_beg), 2, "secs");
				mymodal_span_text.innerHTML = "finished parse_chart_data["+
					gjson.chart_data.length+"] of "+gjson.chrt_data_sz+", "+elap_tm+", tot_elap_tm= "+elap_tm_tot;
				if (gjson.chart_data.length == gjson.chrt_data_sz) {
					setTimeout(function(){ start_charts(); }, g_parse_delay);
				}
			}, g_parse_delay);


			return;
		}
    }; // onmessage

    webSocket.onerror = function(evt) {
        console.log('websocket ERROR: ');
    };

    webSocket.onclose = function(event){
        console.log("Connection closed");
	webSocket.close();
    };
}

function send(){
	//used by the Send button
	var text = document.getElementById("messageinput").value;
	webSocket.send(text);
}
function bootside_menu_setup(div_nm, which_side) {
	$('#'+div_nm).BootSideMenu({

	  // 'left' or 'right'
	  side: which_side,

	  // animation speed
	  duration: 500,

	  // restore last menu status on page refresh
	  remember: true,

	  // auto close
	  autoClose: false,

	  // push the whole page
	  pushBody: true,

	  // close on click
	  closeOnClick: true,

	  // width
	  //width: "25%",
	  //width: "25%",
	width: '350px',

	  // icons
	  icons: {
	    left: 'glyphicon glyphicon-chevron-left',
	    right: 'glyphicon glyphicon-chevron-right',
	    down: 'glyphicon glyphicon-chevron-down'
	  },

	  theme: '',

	});
}
function init_mymodal()
{
	if (mymodal == null) {
		mymodal = document.getElementById("mymodal");
		mymodal_span = document.getElementById("mymodal_close_span");
		mymodal_span_text = document.getElementById("mymodal_span_text");
		//    modal.style.display = "block";

		// When the user clicks on <span> (x), close the modal
		mymodal_span.onclick = function() {
			mymodal.style.display = "none";
		}

		// When the user clicks anywhere outside of the modal, close it
		window.onclick = function(event) {
			if (event.target == mymodal) {
				mymodal.style.display = "none";
			}
		}
	}
}

async function standalone(sp_data2, ch_data2)
{
	let tm_beg = performance.now();
	for (let i=0; i <= 5; i++) {
		if (i != 2 && i != 3) {
			let result = await standaloneJob(i, -1, sp_data2, ch_data2, tm_beg);
		} else {
			for (let j=0; j < ch_data2.length; j++) {
				let result = await standaloneJob(i, j, sp_data2, ch_data2, tm_beg);
			}
		}
	}
}

window.onload = function() {
	update_status("page loaded");
	//  Create an anchor element (note: no need to append this element to the document)
	const url = document.createElement('a');
	console.log("window.location.origin= "+window.location.origin);
	console.log("window.location.port= "+window.location.port);

	init_mymodal();

	// Get the <span> element that closes the modal
	setup_resize_handler();
	bootside_menu_setup('lhs_menu', 'left');
	//bootside_menu_setup('rhs_menu', 'right');
	$('#lhs_menu').BootSideMenu.close();
	openSocket(window.location.port);  // this string is looked for in oppat.cpp
	//$('#rhs_menu').BootSideMenu.close();

}

function setup_resize_handler() {
	var resizeEnd;
	window.addEventListener('resize', function() {
		clearTimeout(resizeEnd);
		resizeEnd = setTimeout(function() {
			// option 1
			let evt = new Event('resize-end');
			window.dispatchEvent(evt);
		}, 250);
	});
}

function get_win_wide_high() {
	var w = window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth;
	var h = window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight;
	return {width: w, height: h};
}

window.addEventListener('resize-end', function() {
	console.log("I've been resized 250ms ago! â€” " + (+new Date));
	let win_sz = get_win_wide_high();
	let update = {
		width: win_sz.width,  // or any new width
		height: gpixels_high_default // " "
	};
	if (typeof gjson == 'undefined' || typeof gjson.chart_data == 'undefined') {
		return;
	}
	for (let i=0; i < gjson.chart_data.length; i++) {
		if (i==0) {
			console.log("doing resize-end loop");
		}
		can_shape(gcanvas_args[i][0], gcanvas_args[i][1], gcanvas_args[i][2], gcanvas_args[i][3], gcanvas_args[i][4], gcanvas_args[i][5], gcanvas_args[i][6], gcanvas_args[i][7], gcanvas_args[i][8], gcanvas_args[i][9]);
	}
});

function clearHoverInfo(ele_nm, this_button_id) {
	console.log("clearHoverInfo ele_nm=" + ele_nm);
	let ele = document.getElementById(ele_nm);
	ele.innerHTML = '';
	ele.clrd = 'y';
	console.log(ele_nm + ".clrd = " + ele.clrd);
	ele = document.getElementById(this_button_id);
	ele.saved_pt = null;
	ele.saved_pt_prev = null;
	ele = document.getElementById('but_'+this_button_id);
	ele.style.visibility = 'hidden';
}

function setClassVisibility(class_nm, visi_val, disp_val) {
	let elems = document.getElementsByClassName(class_nm);
	for (let i = 0; i < elems.length; i++) {
		elems[i].style.visibility = visi_val; // should be hidden or visible
		elems[i].style.display    = disp_val; // should be none or 'inline-block'
	}
}


function showLegend(class_nm, action) {
	//console.log("showLegend class_nm=" + class_nm);
	let top_20      = class_nm+"_legend_top_20";
	let top_20_plus = class_nm+"_legend_top_20_plus";
	//ele = document.getElementByClass(this_button_id);
	if (action == 'show_top_20') {
		setClassVisibility(top_20,      'visible', 'inline-block');
		setClassVisibility(top_20_plus, 'hidden', 'none');
	}
	if (action == 'show_all') {
		setClassVisibility(top_20,      'visible', 'inline-block');
		setClassVisibility(top_20_plus, 'visible', 'inline-block');
	}
	if (action == 'hide_all') {
		setClassVisibility(top_20,      'hidden', 'none');
		setClassVisibility(top_20_plus, 'hidden', 'none');
	}
}

function addElement (ele_typ, new_div_nm, before_div, where) {
	// create a new div element
	let newDiv = document.createElement(ele_typ);
	// and give it some content
	//let newContent = document.createTextNode("Hi there and greetings!");
	// add the text node to the newly created div
	//newDiv.appendChild(newContent);
	newDiv.id = new_div_nm;

	// add the newly created element and its content into the DOM
	let container = document.getElementById('chart_container');
	let anchorDiv = document.getElementById(before_div);
	if (where == 'before') {
		container.insertBefore(newDiv, anchorDiv);
		//document.body.insertBefore(newDiv, anchorDiv);
	} else {
		newDiv.appendAfter(anchorDiv);
		//newDiv.appendBefore(anchorDiv);
		//document.body.insertAfter(newDiv, anchorDiv);
	}
	//document.body.append(newDiv, anchorDiv);
}


function rgbToHex(i, r, g, b) {
	let str = "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
	//console.log("i= "+i+", r= "+r+", g= "+g+", b= "+b+", str= "+str);
	return str;

}

function tm_diff_str(tm_in_secs, precisn, units) {
	let tm_str, tm_val, tm_sfx;
	if (units == 'secs') {
		if (tm_in_secs >= 1.0) {
			tm_val = tm_in_secs;
			tm_sfx = " secs";
		} else if (tm_in_secs >= 1.0e-3) {
			tm_val = 1.0e3 * tm_in_secs;
			tm_sfx = " msecs";
		} else if (tm_in_secs >= 1.0e-6) {
			tm_val = 1.0e6 * tm_in_secs;
			tm_sfx = " usecs";
		} else {
			tm_val = 1.0e9 * tm_in_secs;
			tm_sfx = " nsecs";
		}
	} else if (units == 'nsecs') {
		if (tm_in_secs >= 1.0e9) {
			tm_val = 1.0e-9 * tm_in_secs;
			tm_sfx = " secs";
		} else if (tm_in_secs >= 1.0e6) {
			tm_val = 1.0e-6 * tm_in_secs;
			tm_sfx = " msecs";
		} else if (tm_in_secs >= 1.0e3) {
			tm_val = 1.0e-3 * tm_in_secs;
			tm_sfx = " usecs";
		} else {
			tm_val = tm_in_secs;
			tm_sfx = " nsecs";
		}
	} else  {
		if (tm_in_secs >= 1.0e9) {
			tm_val = 1.0e-9 * tm_in_secs;
			tm_sfx = " G"+units;
		} else if (tm_in_secs >= 1.0e6) {
			tm_val = 1.0e-6 * tm_in_secs;
			tm_sfx = " M"+units;
		} else if (tm_in_secs >= 1.0e3) {
			tm_val = 1.0e-3 * tm_in_secs;
			tm_sfx = " K"+units;
		} else {
			tm_val = tm_in_secs;
			tm_sfx = " "+units;
		}
	}
	tm_str = tm_val.toFixed(precisn);
	let tm0 = tm_str;
	let iln = tm_str.length;
	let p = precisn-1;
	for (let i=iln; i > (iln-p); i--) {
		if (tm_str.substr(i-1, i) == "0") {
			tm_str = tm_str.substr(0, i-1);
		} else {
			break;
		}
	}
	//console.log("tm_diff_str: bef= "+tm0+", aft= "+tm_str);
	return tm_str + " " + tm_sfx;
}

function parse_svg()
{
	//var svgObject = document.getElementById('svg-object').contentDocument;
	var svgObject = document.getElementById('cpu_diagram_svg');
	let shapes = [];
	let svg_gs = svgObject.getElementsByTagName('svg');
	let rect_prev = -1;
	let rects=0;
	let texts=0;
	let svg_xmax = svg_gs[0].getAttributeNS(null, 'width');
	let svg_ymax = svg_gs[0].getAttributeNS(null, 'height');
	let svg_name = svg_gs[0].getAttributeNS(null, 'sodipodi:docname');
	let svg_ver = svg_gs[0].getAttributeNS(null, 'version');
	console.log("svg_name= "+svg_name+', ver= '+svg_ver);

	function svg_parse_path(path, xfrm, rect_prev, shapes, gxf_arr) {
		let xf = path.getAttributeNS(null, 'transform');
		if (xf == '' || xf == null) {
			xf = "";
		}
		let gxf = gxf_arr;
		let d = path.getAttributeNS(null, 'd');
		let style= path.getAttributeNS(null, 'style');
		let id   = path.getAttributeNS(null, 'id');
		let shp = {typ:'path', d:d, style:style, rect_prev:rect_prev, text_arr:[], xf0:[], id:id, xf1:xf, box:null};
		for (let j=0; j < gxf_arr.length; j++) {
			shp.xf0.push(gxf_arr[j]);
		}
		shapes.push(shp);
	}

	function svg_parse_rect(rect, xfrm, rect_prev, shapes, gxf_arr) {
		let x = parseFloat(rect.getAttributeNS(null, 'x'));
		let y = parseFloat(rect.getAttributeNS(null, 'y'));
		let hi= parseFloat(rect.getAttributeNS(null, 'height'));
		let wd= parseFloat(rect.getAttributeNS(null, 'width'));
		let id = rect.getAttributeNS(null, 'id');
		let xf = rect.getAttributeNS(null, 'transform');
		if (xf == '' || xf == null) {
			xf = "";
		}
		let gxf = gxf_arr;
		let style= rect.getAttributeNS(null, 'style');
		if (gxf_arr.length > 0 && gxf_arr[gxf_arr.length-1].substr(0,7) == "matrix(") {
			//console.log("xfrm:rct: "+gxf_arr[gxf_arr.length-1]);
		}
		let shp = {typ:'rect', x:x, y:y, hi:hi, wd:wd, id:id, style:style, rect_prev:rect_prev, text_arr:[], xf0:[], xf1:xf, box:null};
		for (let j=0; j < gxf_arr.length; j++) {
			shp.xf0.push(gxf_arr[j]);
		}
		shapes.push(shp);
		//shapes.push({typ:'rect', x:x, y:y, hi:hi, wd:wd, style:style, rect_prev:rect_prev, xf0:gxf_arr, xf1:xf});
	}

	function svg_parse_text(txt, xfrm, rect_prev, shapes, gxf_arr) {
		let gst = txt.children.length;
		let texts = 0;
		let gxf = gxf_arr;
		let tstyle= txt.getAttributeNS(null, 'style');
		if (tstyle == null) {
			xft = "";
		}
		let xft = txt.getAttributeNS(null, 'transform');
		let gxf_len = gxf.length;
		if (xft == '' || xft == null) {
			xft = "";
		} else {
			gxf.push(xft);
			//console.log("txt_xf: "+xft);
		}
		for (let k=0; k < gst; k++) {
			if (txt.children[k].tagName == "tspan") {
				let x= parseFloat(txt.children[k].getAttributeNS(null, 'x'));
				let y= parseFloat(txt.children[k].getAttributeNS(null, 'y'));
				let style= txt.children[k].getAttributeNS(null, 'style');
				let id   = txt.children[k].getAttributeNS(null, 'id');
				let t    = txt.children[k].innerHTML;
				let xf   = txt.children[k].getAttributeNS(null, 'transform');
				if (xf == '' || xf == null) {
					xf = "";
				}
				//console.log(sprintf("ts[%d].c[%d].c[%d].txt= %s, x= %s", i, j, k, t, x));
				let shp = {typ:'text', x:x, y:y, text:t, id:id, style:style, tstyle:tstyle, rect_prev:rect_prev, xf0:[], xf1:xf, box:null};
				for (let j=0; j < gxf.length; j++) {
					shp.xf0.push(gxf[j]);
				}
				if (rect_prev >= 0) {
					shapes[rect_prev].text_arr.push(shapes.length);
				}
				shapes.push(shp);
				texts++;
			}
		}
		while (gxf.length > gxf_len) {
			gxf.pop();
		}
		return texts;
	}

	function svg_parse_g(ge, shapes, cntr, gxf_arr) {
		let xfrm = ge.getAttributeNS(null, 'transform');
		if (xfrm == '' || xfrm == null) {
			xfrm = "";
		} else {
			if (xfrm.length > 7 && xfrm.substr(0,7) == "matrix(") {
				//console.log("xfrm= "+xfrm);
			}
		}
		let gxf_arr_len = gxf_arr.length;
		gxf_arr.push(xfrm);
		let gsc = ge.children.length;
		let xf_dmy = "";
		for (let j=0; j < gsc; j++) {
			if (ge.children[j].tagName == "g") {
				svg_parse_g(ge.children[j], shapes, cntr, gxf_arr);
			}
			if (ge.children[j].tagName == "rect") {
				svg_parse_rect(ge.children[j], xf_dmy, rect_prev, shapes, gxf_arr);
				rect_prev = shapes.length-1;
				cntr.rects++;
			}
			if (ge.children[j].tagName == "text") {
				cntr.texts += svg_parse_text(ge.children[j], xf_dmy, rect_prev, shapes, gxf_arr);
			}
			if (ge.children[j].tagName == "path") {
				cntr.paths += svg_parse_path(ge.children[j], xf_dmy, rect_prev, shapes, gxf_arr);
			}
		}
		while(gxf_arr.length > gxf_arr_len) {
			gxf_arr.pop();
		}
		xfrm = "";
	}

	let cntr = {texts:0, rects:0, paths:0};

	let gxf_arr = [];
	for (let i=0; i < svg_gs[0].children.length; i++) {
		let ge = svg_gs[0].children[i];
		if (ge.tagName == "g") {
			//console.log(sprintf("g[%d].id= %s", i, ge.id));
			svg_parse_g(ge, shapes, cntr, gxf_arr);
		}
		if (ge.tagName == "rect") {
			svg_parse_rect(ge, "", rect_prev, shapes, gxf_arr);
			rect_prev = shapes.length-1;
			console.log("rect id= %s"+ge.id);
			cntr.rects++;
		}
		if (ge.tagName == "text") {
			cntr.texts += svg_parse_text(ge, "", rect_prev, shapes, gxf_arr);
		}
		//console.log(sprintf("ele[%d].id= %s", i, ge.id));
	}
	svgObject.style.display = 'none';
	console.log(sprintf("rects= %d, texts= %d", cntr.rects, cntr.texts));
	console.log(sprintf("svg: width= %s, height= %s", svg_xmax, svg_ymax));
	//abcd
	let hvr_clr = 'mysvg';
	let win_sz = get_win_wide_high();
	let px_wide = win_sz.width - 30;
	let scale_ratio = px_wide/svg_xmax;
	let px_high = svg_ymax/svg_xmax * px_wide;
	let myhvr_clr = document.getElementById(hvr_clr);
	let str = '<div id="tbl_'+hvr_clr+'"></div><div class="tooltip"><canvas id="canvas_'+hvr_clr+'" width="'+(px_wide)+'" height="'+(px_high)+'" style="border:1px solid #000000;"><span class="tooltiptext" id="tooltip_'+hvr_clr+'"></span></div>';
	myhvr_clr.innerHTML = str;
	let mycanvas  = document.getElementById('canvas_'+hvr_clr);
	let mytooltip = document.getElementById("tooltip_"+hvr_clr);
	let mytable   = document.getElementById("tbl_"+hvr_clr);
	let tt_prv_x=-2, tt_prv_y=-2;
	let ctx = mycanvas.getContext("2d");

	function xlate_rotate(xin, yin, degrees) {
		let xout = xin;
		let yout = yin;
		let angle = parseFloat(degrees);
		angle = (angle/90.0) * 0.5*Math.PI;
		let s = Math.sin(angle);
		let c = Math.cos(angle);
		let xnew = xout * c - yout * s;
		let ynew = xout * s + yout * c;
		//console.log(sprintf("xo= %f, xn= %f yo= %f, yn= %f", xout, xnew, yout, ynew));
		xout = xnew;
		yout = ynew;
		return [xout, yout];
	}
	function decode_xform(xin, yin, xform, verbose) {
		let xout = xin;
		let yout = yin;
		if (xform.length > 7 && xform.substr(0,7) == "rotate(") {
			let xf = xform.substr(7, xform.length-8);
			if (verbose) {
				console.log("rotate angle= "+xf);
			}
			[xout, yout] = xlate_rotate(xout, yout, xf);
		}
		if (xform.length > 6 && xform.substr(0,6) == "scale(") {
			let xf = xform.substr(6, xform.length-7);
			let scl = xf.split(",");
			if (scl.length == 1) {
				scl.push(scl[0]);
			}
			scl[0] = parseFloat(scl[0]);
			scl[1] = parseFloat(scl[1]);
			let xn= xout * scl[0];
			let yn= yout * scl[1];
			if (verbose) {
				console.log(sprintf("scl: xo/n= '%f/%f', y='%f/%f'", xout, xn, yout, yn));
			}
			xout = xn;
			yout = yn;
			if (verbose != 1) {
			}
		}
		if (xform.length > 10 && xform.substr(0,10) == "translate(") {
			let xf = xform.substr(10, xform.length-11);
			let scl = xf.split(",");
			if (scl.length == 1) {
				//console.log("add y0 for xf= "+xf);
				scl.push("0.0");
			}
			scl[0] = parseFloat(scl[0]);
			scl[1] = parseFloat(scl[1]);
			let xn= xout + scl[0];
			let yn= yout + scl[1];
			if (verbose) {
				console.log(sprintf("xlt: xo/n= '%f/%f', y='%f/%f'", xout, xn, yout, yn));
			}
			xout = xn;
			yout = yn;
		}
		if (xform.length > 7 && xform.substr(0,7) == "matrix(") {
			// newX = a * oldX + c * oldY + e
			// newY = b * oldX + d * oldY + f
			let xf = xform.substr(7, xform.length-8);
			let scl = xf.split(",");
			if (verbose) {
				console.log(sprintf("got matrix.len %d, %s", scl.length, xform));
			}
			for (let i=0; i < scl.length; i++) {
				scl[i] = parseFloat(scl[i]);
			}
			let xn= scl[0] * xout + scl[2] * yout + scale_ratio * scl[4];
			let yn= scl[1] * xout + scl[3] * yout + scale_ratio * scl[5];
			if (verbose) {
				console.log(sprintf("xlt: xo/n= '%f/%f', y='%f/%f'", xout, xn, yout, yn));
			}
			xout = xn;
			yout = yn;
			/*
			*/
		}
		return [xout, yout];
	}
	function xlate(tctx, xin, yin, uminx, umaxx, uminy, umaxy, shape) {
		//let xout = Math.trunc(px_wide * (xin - uminx)/ (umaxx - uminx));
		//let yout = Math.trunc(px_high * (yin - uminy)/ (umaxy - uminy));
		let xout = px_wide * (xin - uminx)/ (umaxx - uminx);
		let yout = px_high * (yin - uminy)/ (umaxy - uminy);
		let verbose = 0;
		if (shape != null) {
			let id = shape.id;
			if (id == 'rect7914') {
				//console.log("xlate beg id= "+id);
				//verbose = 1;
			}
			if (shape.xf1 != "") {
				[xout, yout] = decode_xform(xout, yout, shape.xf1, verbose);
			}
			if (shape.xf0.length > 0) {
				for (let i=shape.xf0.length-1; i >=0; i--) {
					[xout, yout] = decode_xform(xout, yout, shape.xf0[i], verbose);
				}
			}
		}
		return [xout, yout];
	}

	// transform matrix
	// [a c e]
	// [b d f]
	// [0 0 1]
	// newX = a * oldX + c * oldY + e
	// newY = b * oldX + d * oldY + f
	// The translate(<x> [<y>]) transform function moves the object by x and y (i.e. xnew = xold + <x>, ynew = yold + <y>). If y is not provided, it is assumed to be zero.
	// The scale(<x> [<y>]) transform function specifies a scale operation by x and y. If y is not provided, it is assumed to be equal to x.
	// The rotate(<a> [<x> <y>]) transform function specifies a rotation by a degrees about a given point.
	// If optional parameters x and y are not supplied, the rotation is about the origin of the current
	// user coordinate system. If optional parameters x and y are supplied, the rotate is about the point (x, y).

	ctx.clearRect(0, 0, mycanvas.width, mycanvas.height);
	// draw yaxis label
	let step = [];
	ctx.fillStyle = 'black';
	// draw y by_var values.
	let font_sz = 11.0;
	ctx.font = font_sz + 'px Arial';
	ctx.textAlign = "right";

	function pair2pt(str) {
		let arr = str.split(",");
		let x = parseFloat(arr[0]);
		let y = 0.0;
		if (arr.length > 1) {
			y = parseFloat(arr[1]);
		}
		return {x:x, y:y};
	}

	// begin find end-points
	//let lkfor_id = 'path4777';
	//let lkfor_id = 'path4281';
	//let lkfor_id = 'path4688';
	//let lkfor_id = 'path4188';
	//let lkfor_id = 'path4760';
	//let lkfor_id = 'path4824';
	//let lkfor_id = 'pathxxxx';
	//let lkfor_id = 'path4748';
	//	let lkfor_id = 'path4188';
	let lkfor_id = "path4651";

	function build_poly() {
		let pt  = {x:0, y:0};
		for (let i=0; i < shapes.length; i++) {
			if (shapes[i].typ != 'path') {
				continue;
			}
			let vrb = 0;
			if (shapes[i].id == lkfor_id) {
				vrb = 1;
			}
			shapes[i].poly = [];
			for (let j=0; j < shapes[i].cmds.length; j++) {
				pt.x = shapes[i].cmds[j].pt_a[0].x;
				pt.y = shapes[i].cmds[j].pt_a[0].y;
				let p0 = xlate(ctx, pt.x, pt.y, 0, svg_xmax, 0, svg_ymax, shapes[i]);
				shapes[i].poly.push({x:p0[0], y:p0[1]});
				if (shapes[i].cmds[j].cmd == 'C' || shapes[i].cmds[j].cmd == 'c') {
					p0 = xlate(ctx, shapes[i].cmds[j].pt_a[2].x, shapes[i].cmds[j].pt_a[2].y, 0, svg_xmax, 0, svg_ymax, shapes[i]);
					shapes[i].poly.push({x:p0[0], y:p0[1]});
				}
			}
		}
	}

	function decode_paths() {
		for (let i=0; i < shapes.length; i++) {
			if (shapes[i].typ != 'path') {
				continue;
			}
			let arr = shapes[i].d.split(" ");
			shapes[i].cmds = [];
			let last_cmd = "";
			let j = 0;

			while (j < arr.length) {
				if (arr[j] == "M" || arr[j] == "m" || arr[j] == "L" || arr[j] == "l" ||
					arr[j] == "H" || arr[j] == "h" || arr[j] == "V" || arr[j] == "v" ||
					arr[j] == "c" || arr[j] == "C" || arr[j] == "Z" || arr[j] == "z") {
					last_cmd = arr[j];
					if (arr[j] == "M" || arr[j] == "m" || arr[j] == "L" || arr[j] == "l" ||
						arr[j] == "H" || arr[j] == "h" || arr[j] == "V" || arr[j] == "v") {
						shapes[i].cmds.push({cmd:arr[j], pt:[pair2pt(arr[j+1])]});
						j += 2;
						continue;
					}
					if (arr[j] == "Z" || arr[j] == "z") {
						shapes[i].cmds.push({cmd:arr[j]});
						j++;
						continue;
					}
					if (arr[j] == "c" || arr[j] == "C") {
						shapes[i].cmds.push({cmd:arr[j], pt:[pair2pt(arr[j+1]), pair2pt(arr[j+2]), pair2pt(arr[j+3])]});
						j += 4;
						continue;
					}
				}
				if (last_cmd == "M") {
					shapes[i].cmds.push({cmd:"L", pt:[pair2pt(arr[j])]});
					last_cmd = "L";
					j++;
					continue;
				}
				if (last_cmd == "m") {
					shapes[i].cmds.push({cmd:"l", pt:[pair2pt(arr[j])]});
					last_cmd = "l";
					j++;
					continue;
				}
				if (last_cmd == "L" || last_cmd == "l") {
					shapes[i].cmds.push({cmd:last_cmd, pt:[pair2pt(arr[j])]});
					j++;
					continue;
				}
				if (last_cmd == "H" || last_cmd == "h") {
					shapes[i].cmds.push({cmd:last_cmd, pt:[pair2pt(arr[j])]});
					j++;
					continue;
				}
				if (last_cmd == "v" || last_cmd == "V") {
					shapes[i].cmds.push({cmd:last_cmd, pt:[pair2pt(arr[j])]});
					j++;
					continue;
				}
				if (last_cmd == "C" || last_cmd == "c") {
					shapes[i].cmds.push({cmd:last_cmd, pt:[pair2pt(arr[j]), pair2pt(arr[j+1]), pair2pt(arr[j+2])]});
					j += 3;
					continue;
				}
				if (last_cmd == "Z" || last_cmd == "z") {
					shapes[i].cmds.push({cmd:last_cmd});
					j++;
					continue;
				}
			}
		}
		for (let i=0; i < shapes.length; i++) {
			if (shapes[i].typ != 'path') {
				continue;
			}
			let pt = {x:null, y:null}; // prev pt
			let bg = {x:null, y:null}; // beginning pt (needed to close the shape with Z cmd)
			for (let j=0; j < shapes[i].cmds.length; j++) {
				let typ = shapes[i].cmds[j].cmd;
				shapes[i].cmds[j].pt_a = [];
				if (typ == 'M' || typ == 'm') {
					// it seems that the 1st cmd is alway M or m and,
					// if it is the 1st point then the x,y pair is always absolute (even if cmd == 'm')
					let x = shapes[i].cmds[j].pt[0].x;
					let y = shapes[i].cmds[j].pt[0].y;
					if (j == 0) {
						bg.x = x;
						bg.y = y;
					} else if (typ == 'm') {
						x += pt.x;
						y += pt.y;
					}
					shapes[i].cmds[j].pt_a.push({x:x, y:y});
					pt.x = x;
					pt.y = y;
					continue;
				}
				if (typ == 'Z' || typ == 'z') {
					let x = bg.x;
					let y = bg.y;
					shapes[i].cmds[j].pt_a.push({x:x, y:y});
					pt.x = x;
					pt.y = y;
					continue;
				}
				if (typ == 'C' || typ == 'c') {
					let x = shapes[i].cmds[j].pt[0].x;
					let y = shapes[i].cmds[j].pt[0].y;
					if (typ == 'c') {
						x += pt.x;
						y += pt.y;
					}
					shapes[i].cmds[j].pt_a.push({x:x, y:y});
					x = shapes[i].cmds[j].pt[1].x;
					y = shapes[i].cmds[j].pt[1].y;
					if (typ == 'c') {
						x += pt.x;
						y += pt.y;
					}
					shapes[i].cmds[j].pt_a.push({x:x, y:y});
					x = shapes[i].cmds[j].pt[2].x;
					y = shapes[i].cmds[j].pt[2].y;
					if (typ == 'c') {
						x += pt.x;
						y += pt.y;
					}
					shapes[i].cmds[j].pt_a.push({x:x, y:y});
					pt.x = x;
					pt.y = y;
					continue;
				}
				if (typ == 'L' || typ == 'l') {
					let x = shapes[i].cmds[j].pt[0].x;
					let y = shapes[i].cmds[j].pt[0].y;
					if (typ == 'l') {
						x += pt.x;
						y += pt.y;
					}
					shapes[i].cmds[j].pt_a.push({x:x, y:y});
					pt.x = x;
					pt.y = y;
					continue;
				}
				if (typ == 'H' || typ == 'h') {
					let x = shapes[i].cmds[j].pt[0].x;
					let y = pt.y
					if (typ == 'h') {
						x += pt.x;
					}
					shapes[i].cmds[j].pt_a.push({x:x, y:y});
					pt.x = x;
					pt.y = y;
					continue;
				}
				if (typ == 'V' || typ == 'v') {
					let x = pt.x
					let y = shapes[i].cmds[j].pt[0].x; // yes, just x (only 1 value for HhVv cmds)
					if (typ == 'v') {
						y += pt.y;
					}
					shapes[i].cmds[j].pt_a.push({x:x, y:y});
					pt.x = x;
					pt.y = y;
					continue;
				}
			}
		}
	}

	function rot_pt(cx, cy, x, y, angle, rot) {
		let radians;
		if (angle != 0.0) {
			radians = angle;
		} else {
			radians = rot * Math.PI/2;
		}
		let cos = Math.cos(radians);
		let sin = Math.sin(radians);
		let nx = (cos * (x - cx)) + (sin * (y - cy)) + cx;
		let ny = (cos * (y - cy)) - (sin * (x - cx)) + cy;
		return [nx, ny];
	}


	decode_paths();

	function draw_lineTo(cmd_typ, ctx, x, y, shape) {
		if (cmd_typ != 'cpy') {
			let p0 = xlate(ctx, x, y, 0, svg_xmax, 0, svg_ymax, shape);
			if (cmd_typ == 'M' || cmd_typ == 'm') {
				ctx.moveTo(p0[0], p0[1]);
			} else {
				ctx.lineTo(p0[0], p0[1]);
			}
		}
		return {x:x, y:y};
	}

	function ck_if_i_in_list(lkfor, arr) {
		if (arr.length > 0 && arr.indexOf(lkfor) > -1) {
			return true;
		}
		return false;
	}

	function follow_paths2(i, typ, poly) {
		for (let j=0; j < shapes[i].poly.length; j++) {
			if (typeof shapes[i].poly[j].arrow_extended != 'undefined') {
				add_path_connections(i, 'path', poly, shapes[i].poly[j].x, shapes[i].poly[j].y);
			} else if (typeof shapes[i].poly[j].endpoint_line != 'undefined') {
				add_path_connections(i, 'path', poly, shapes[i].poly[j].endpoint_line.x0, shapes[i].poly[j].endpoint_line.y0);
			}
		}
	}

	function add_path_connections(i, typ, poly, x, y) {
		let arr2 = find_overlap(x, y);
		if (typeof poly.im_connected_to == 'undefined') {
			poly.im_connected_to = [];
		}
		if (shapes[i].id == lkfor_id) {
			//console.log(sprintf("apc: id=%s", lkfor_id));
		}
		for (let k=arr2.length-1; k >= 0; k--) {
			if (arr2[k] < 0) {
				continue;
			}
			if (arr2[k] != i && shapes[arr2[k]].typ == typ) {
				let kk = poly.im_connected_to.indexOf(arr2[k]);
				if (kk == -1) {
					poly.im_connected_to.push(arr2[k]);
					if (typeof shapes[arr2[k]].is_connected_to_me == 'undefined') {
						shapes[arr2[k]].is_connected_to_me = [];
					}
					if (shapes[arr2[k]].is_connected_to_me.indexOf(i) == -1) {
						shapes[arr2[k]].is_connected_to_me.push(i);
					}
				}
				if (typ == 'rect') {
					break; // just add the first (highest-level) rectangle
				}
				if (shapes[arr2[k]].id == lkfor_id) {
					//console.log(sprintf("apc: cf= %s to=%s", shapes[i].id, lkfor_id));
				}
				/*
				if (typ == 'path') {
					follow_paths2(arr2[k], typ, poly);
				}
				*/
			}
		}
	}

	function draw_svg(hilite_arr) {
		build_poly();
		ctx.clearRect(0, 0, mycanvas.width, mycanvas.height);
		let drew_rect = 0;
		let drew_text = 0;
		let drew_path = 0;
		for (let i=0; i < shapes.length; i++) {
			let highlight = ck_if_i_in_list(i, hilite_arr);
			if (shapes[i].typ == 'path') {
				let style = shapes[i].style;
				let st = style.split(";");
				let fill_clr = null;
				let strk_clr = null;
				shapes[i].box = {xb:0, xe:0, yb:1, ye:1, str:"path "+i};
				for (let j=0; j < st.length; j++) {
					if (st[j].length > 5 && st[j].substr(0,5) == "fill:") {
						fill_clr = st[j].substr(5);
					} else if (st[j].length > 7 && st[j].substr(0,7) == "stroke:") {
						strk_clr = st[j].substr(7);
					}
				}
				let pt = {x:null, y:null}; // prev pt
				let bg = {x:null, y:null}; // beginning pt (needed to close the shape with Z cmd)
				ctx.beginPath();
				let line_wd = 1;
				if (highlight) {
					strk_clr = 'black';
					line_wd = 3;
				}
				ctx.lineWidth = line_wd;
				//console.log("cmds.len= "+shapes[i].cmds.length);
				for (let j=0; j < shapes[i].cmds.length; j++) {
					let typ = shapes[i].cmds[j].cmd;
					if ((typ == 'M' || typ == 'm')) {
						// it seems that the 1st cmd is alway M or m and,
						// if it is the 1st point then the x,y pair is always absolute (even if cmd == 'm')
						let x = shapes[i].cmds[j].pt[0].x;
						let y = shapes[i].cmds[j].pt[0].y;
						if (j == 0) {
							bg.x = x;
							bg.y = y;
						} else if (typ == 'm') {
							x += pt.x;
							y += pt.y;
						}
						pt = draw_lineTo(typ, ctx, x, y, shapes[i]);
						continue;
					}
					if (typ == 'Z' || typ == 'z') {
						let x = bg.x;
						let y = bg.y;
						pt = draw_lineTo(typ, ctx, x, y, shapes[i]);
						continue;
					}
					if (typ == 'C' || typ == 'c') {
						let x = shapes[i].cmds[j].pt[0].x;
						let y = shapes[i].cmds[j].pt[0].y;
						if (typ == 'c') {
							x += pt.x;
							y += pt.y;
						}
						let p0 = xlate(ctx, x, y, 0, svg_xmax, 0, svg_ymax, shapes[i]);
						x = shapes[i].cmds[j].pt[1].x;
						y = shapes[i].cmds[j].pt[1].y;
						if (typ == 'c') {
							x += pt.x;
							y += pt.y;
						}
						let p1 = xlate(ctx, x, y, 0, svg_xmax, 0, svg_ymax, shapes[i]);
						x = shapes[i].cmds[j].pt[2].x;
						y = shapes[i].cmds[j].pt[2].y;
						if (typ == 'c') {
							x += pt.x;
							y += pt.y;
						}
						let p2 = xlate(ctx, x, y, 0, svg_xmax, 0, svg_ymax, shapes[i]);
						ctx.bezierCurveTo(p0[0], p0[1], p1[0], p1[1], p2[0], p2[1]);
						//console.log(sprintf("%s %f %f (%f %f)", typ, x, y, p0[0], p0[1]));
						pt.x = x;
						pt.y = y;
						continue;
					}
					if (typ == 'L' || typ == 'l') {
						let x = shapes[i].cmds[j].pt[0].x;
						let y = shapes[i].cmds[j].pt[0].y;
						if (typ == 'l') {
							x += pt.x;
							y += pt.y;
						}
						pt = draw_lineTo(typ, ctx, x, y, shapes[i]);
						continue;
					}
					if (typ == 'H' || typ == 'h') {
						let x = shapes[i].cmds[j].pt[0].x;
						let y = pt.y
						if (typ == 'h') {
							x += pt.x;
						}
						pt = draw_lineTo(typ, ctx, x, y, shapes[i]);
						continue;
					}
					if (typ == 'V' || typ == 'v') {
						let x = pt.x
						let y = shapes[i].cmds[j].pt[0].x; // yes, just x (only 1 value for HhVv cmds)
						if (typ == 'v') {
							y += pt.y;
						}
						pt = draw_lineTo(typ, ctx, x, y, shapes[i]);
						continue;
					}
				}
				if (strk_clr != 'none') {
					ctx.strokeStyle = strk_clr;
					ctx.stroke();
				}
				if (fill_clr != 'none') {
					ctx.fillStyle = fill_clr;
					ctx.fill();
				}
				ctx.closePath();
				ctx.beginPath();

				let ppt = {x:0, y:0};
				let do_draw = -1;
				shapes[i].angles = [];


				for (let j=0; j < shapes[i].poly.length-1; j++) {
					if (true && shapes[i].id == lkfor_id)
					{
						let x = shapes[i].poly[j].x;
						let y = shapes[i].poly[j].y;
						ctx.strokeStyle = 'black';
						ctx.font = sprintf("%.3fpx sans-serif", 9);
						let fl = ctx.fillStyle;
						ctx.fillStyle = 'black';
						ctx.fillText(j, x, y);
						ctx.fillStyle = fl;
						//let fl = ctx.strokeStyle = 'black';
					}
					let jm1 = j-1;
					let jp1 = j+1;
					if (j == 0) {
						jm1 = shapes[i].poly.length - 2;
					} else if (jp1 == shapes[i].poly.length) {
						jp1 = 1;
					}
					let ori = {x:shapes[i].poly[j  ].x, y:shapes[i].poly[j  ].y};
					let v1  = {x:shapes[i].poly[jm1].x - ori.x, y:shapes[i].poly[jm1].y - ori.y};
					let v2  = {x:shapes[i].poly[jp1].x - ori.x, y:shapes[i].poly[jp1].y - ori.y};
					//let angle = Math.atan2(v2.y - ori.y, v2.x - ori.x) - Math.atan2(v1.y-ori.y, v1.x-ori.x);
					//angle *=  180.0 / Math.PI;
					//cos(angle) = dot(v1,v2) / (len(v1)*len(v2))
					let dot = v1.x * v2.x + v1.y * v2.y;
					let v1_len =   Math.sqrt(v1.x * v1.x + v1.y * v1.y);
					let v2_len =   Math.sqrt(v2.x * v2.x + v2.y * v2.y);
					shapes[i].poly[j].m1_len = v1_len;
					shapes[i].poly[j].p1_len = v2_len;
					let den = v1_len * v2_len;
					let angle = 0.0;
					if (den != 0.0) {
						angle = Math.acos(dot/den);
						angle *= 180.0 / Math.PI;
					}
					if (angle > 135.0 && angle < 175.0)
					{
						if (shapes[i].id == lkfor_id) {
							console.log(sprintf("w[%d] angle= %.3f", j, angle));
						}
					}
					if (89.0 < angle && angle < 91.0) {
						angle = 90.0;
					}
					if (angle > 135.0 && angle < 175.0) {
						angle -= 90;
					}
					shapes[i].angles.push(angle);
					let str_angle = sprintf("%.3f", angle);
					if (shapes[i].id == lkfor_id) {
						//console.log(sprintf("j[%d] angle= %.3f rght=%s", j, angle, (angle==90.0?1:0)));
					}
				}
				ctx.lineWidth = 1;
				drew_path++;
			}
			if (shapes[i].typ == 'rect') {
				let x0 = shapes[i].x;
				let y0 = shapes[i].y;
				let x1 = x0 + shapes[i].wd;
				let y1 = y0 + shapes[i].hi;
				let style = shapes[i].style;
				let st = style.split(";");
				let fill_clr = null;
				let strk_clr = null;
				for (let j=0; j < st.length; j++) {
					if (st[j].length > 5 && st[j].substr(0,5) == "fill:") {
						fill_clr = st[j].substr(5);
					} else if (st[j].length > 7 && st[j].substr(0,7) == "stroke:") {
						strk_clr = st[j].substr(7);
					}
				}
				let p0 = xlate(ctx, x0, y0, 0, svg_xmax, 0, svg_ymax, shapes[i]);
				let p1 = xlate(ctx, x1, y1, 0, svg_xmax, 0, svg_ymax, shapes[i]);
				ctx.beginPath();
				ctx.fillStyle = 'blue';
				let wd = p1[0] - p0[0];
				let hi = p1[1] - p0[1];
				shapes[i].box = {xb:p0[0], xe:p1[0], yb:p0[1], ye:p1[1], str:"rect "+i};
				shapes[i].poly = [];
				shapes[i].poly.push({x:p0[0], y:p0[1]});
				shapes[i].poly.push({x:p1[0], y:p0[1]});
				shapes[i].poly.push({x:p1[0], y:p1[1]});
				shapes[i].poly.push({x:p0[0], y:p1[1]});
				shapes[i].poly.push({x:p0[0], y:p0[1]});
				if (shapes[i].id == 'rect4213') {
					//console.log(sprintf("r[%d]: x0= %s, x1= %s y0= %s y1= %s, p0= %s p1= %s wd= %s, hi= %s",
					//			i, x0, x1, y0, y1, p0, p1, shapes[i].wd, shapes[i].hi));
				}
				if (fill_clr != null) {
					ctx.fillStyle = fill_clr;
					ctx.fillRect(p0[0],p0[1], wd, hi);
				}
				if (highlight) {
					strk_clr = 'black';
					ctx.lineWidth = 3;
				}
				if (strk_clr != null) {
					ctx.strokeStyle = strk_clr;
					ctx.strokeRect(p0[0],p0[1], wd, hi);
				}
				if (highlight) {
					ctx.lineWidth = 1;
				}
				drew_rect++;
			}
			if (shapes[i].typ == 'text') {
					//let shp = {typ:'text', x:x, y:y, text:t, style:style, rect_prev:rect_prev, xf0:[], xf1:xf};
				//style="font-size:7.82361031px;line-height:1.25;font-family:sans-serif;stroke-width:1.26729274"
				//text-align:center;letter-spacing:0px;word-spacing:0px;text-anchor:middle
				//let x1 = x0 + shapes[i].wd;
				//let y1 = y0 + shapes[i].hi;
				let tstyle = shapes[i].tstyle;
				let tst = tstyle.split(";");
				let text_align = 'left';
				let text_anchor = 'bottom';
				for (let j=0; j < tst.length; j++) {
					if (tst[j].length > 11 && tst[j].substr(0,11) == "text-align:") {
						text_align = tst[j].substr(11);
					}
					if (tst[j].length > 12 && tst[j].substr(0,12) == "text-anchor:") {
						text_anchor = tst[j].substr(12);
					}
				}
				let style = shapes[i].style;
				let st = style.split(";");
				let nfont_sz = 11;
				let line_hi = 0;
				let font_sz = "11px";
				for (let j=0; j < st.length; j++) {
					if (st[j].length > 5 && st[j].substr(0,10) == "font-size:") {
						font_sz = st[j].substr(10);
						nfont_sz = parseFloat(font_sz.substr(0, font_sz.length-2));
					}
					if (st[j].length > 12 && st[j].substr(0,12) == "line-height:") {
						line_hi = st[j].substr(12);
						if (line_hi.indexOf('%') > 0) {
							line_hi = line_hi.replace('%', '');
							line_hi = parseFloat(line_hi);
							if (line_hi >= 100.0) {
								line_hi /= 100.0;
							}
						} else {
							line_hi = parseFloat(line_hi);
						}
					}
				}
				// if we've scaled down the canvas sz from svg sz then font can be too big to fit in box
				nfont_sz *= scale_ratio;
				shapes[i].nfont_sz = nfont_sz;
				let x0 = shapes[i].x;
				let y0 = shapes[i].y;
				if (line_hi > 1.0) {
					y0 -= nfont_sz * (line_hi - 1.0);
				}
				//let y0 = shapes[i].y - nfont_sz;
				let p0 = xlate(ctx, x0, y0, 0, svg_xmax, 0, svg_ymax, shapes[i]);
				//let p1 = xlate(ctx, x1, y1, 0, svg_xmax, 0, svg_ymax, shapes[i]);
				if (line_hi > 1.0) {
					//p0[1] -= nfont_sz * (line_hi - 1.0);
				}
				let rot = 0;
				let angle = 0.0;
				for (let j=0; j < shapes[i].xf0.length; j++) {
					if (shapes[i].xf0[j].length > 7 && shapes[i].xf0[j].substr(0,7) == "rotate(") {
						let xf = shapes[i].xf0[j].substr(7, shapes[i].xf0[j].length-8);
						xf = parseFloat(xf)/90.0;
						rot = xf;
					}
					if (shapes[i].xf0[j].indexOf("matrix") >= 0) {
						let arr = shapes[i].xf0[j].substr(7, shapes[i].xf0[j].length-8).split(',');
						let b = parseFloat(arr[1]);
						let c = parseFloat(arr[2]);
						let ub = 0;
						let uc = 0;
						//let xnew = xout * c - yout * s;
						//rot = angle/(0.5*Math.PI);
						//console.log(sprintf("sin()= %f, angle= %f, pi/2= %f, rot= %f", b, angle, Math.PI/2, rot));
						if (0.99 <= b && b <= 1.01) {
							ub = 1;
						} else if (-1.01 <= b && b <= -0.99) {
							ub = -1;
						}
						if (0.99 <= c && c <= 1.01) {
							uc = 1;
						} else if (-1.01 <= c && c <= -0.99) {
							uc = -1;
						}
						if (ub == 1 && uc == -1) {
							rot = 1;
						}
						if (ub == -1 && uc == 1) {
							rot = -1;
						}
						if (!(rot == 1 || rot == 0 || rot == -1)) {
							angle = Math.asin(b);
						}
					}
				}
				if (shapes[i].text == "Adder") {
					//console.log(shapes[i].text+":");
					//console.log(shapes[i]);
				}
				ctx.beginPath();
				ctx.textAlign = text_align;
				ctx.textBaseline = text_anchor;
				ctx.fillStyle = 'black';
				//ctx.font = font_sz + ' Arial';
				ctx.font = sprintf("%.3fpx sans-serif", nfont_sz);
				//ctx.font = font_sz + ' sans-serif';
				let str = shapes[i].text;
				if (str.indexOf('&amp;') >= 0) {
					str = str.replace('&amp;', '&');
				}
				let twidth = ctx.measureText(str).width;
				let xb, xe, yb, ye;
				if (text_align == 'start' || text_align == 'left') {
					xb = p0[0];
					xe = xb + twidth;
				} else if (text_align == 'end' || text_align == 'right') {
					xe = p0[0];
					xb = xe - twidth;
				} else if (text_align == 'center') {
					xb = p0[0] - 0.5*twidth;
					xe = p0[0] + 0.5*twidth;
				} else {
					console.log("unknown text_align= "+text_align);
					xb = p0[0];
					xe = xb + twidth;
				}
				if (text_anchor == 'top' || text_anchor == 'hanging') {
					yb = p0[1];
					ye = yb - nfont_sz;
				} else if (text_anchor == 'bottom' || text_anchor == 'alphabetic') {
					ye = p0[1];
					yb = ye - nfont_sz;
				} else if (text_anchor == 'middle') {
					yb = p0[1] - 0.5*nfont_sz;
					ye = p0[1] + 0.5*nfont_sz;
				} else {
					console.log("unknown text_anchor= "+text_anchor);
					ye = p0[1];
					yb = ye - nfont_sz;
				}
				shapes[i].box = {xb:xb, xe:xe, yb:yb, ye:ye, str:str};
				//console.log(sprintf("txt %s %s", text_align, text_anchor));
				//console.log(shapes[i].box);

				if (rot != 0) {
					ctx.save();
					ctx.translate(p0[0], p0[1]);
					if (angle != 0.0) {
						ctx.rotate(angle);
					} else {
						ctx.rotate(rot * Math.PI/2);
					}
					let npt0 = rot_pt(p0[0], p0[1], shapes[i].box.xb, shapes[i].box.yb, angle, rot);
					let npt1 = rot_pt(p0[0], p0[1], shapes[i].box.xe, shapes[i].box.ye, angle, rot);
					shapes[i].box.xb = npt0[0];
					shapes[i].box.yb = npt0[1];
					shapes[i].box.xe = npt1[0];
					shapes[i].box.ye = npt1[1];

					ctx.fillText(str, 0, 0);
					ctx.restore()
				} else {
					ctx.fillText(str, p0[0], p0[1]);
				}
				drew_text++;
			}
		}
		for (let i=0; i < shapes.length; i++) {
			if (shapes[i].typ == 'path') {
				let draw_circles = false;
				for (let j=0; j < shapes[i].angles.length; j++) {
					//angle = atan2(vector2.y, vector2.x) - atan2(vector1.y, vector1.x);
					//if (angle < 0) angle += 2 * M_PI;
					let jm2 = j-2;
					let jm1 = j-1;
					let jp1 = j+1;
					let jp2 = j+2;
					if (j == 0) {
						jm2 = shapes[i].angles.length - 2;
						jm1 = shapes[i].angles.length - 1;
					} else if (j == 1) {
						jm2 = shapes[i].angles.length - 1;
					} else if ((j+1) == shapes[i].angles.length) {
						jp1 = 0;
						jp2 = 1;
					} else if ((j+2) == shapes[i].angles.length) {
						jp2 = 0;
					}
					//let angle = sprintf("%.3f", shapes[i].angles[j]+shapes[i].angles[jm1]+shapes[i].angles[jp1]);
					let angle = shapes[i].angles[j]+shapes[i].angles[jm1]+shapes[i].angles[jp1];
					let angle2 = angle;
					if (((176 <= angle && angle <= 181) || (176 <= angle2 && angle2 <= 181)) &&
							shapes[i].angles[jm2] == 90.0 && shapes[i].angles[jp2]== 90.0) {
						ctx.strokeStyle = 'black';
						ctx.beginPath();
						if (draw_circles) {
							ctx.arc(shapes[i].poly[j].x, shapes[i].poly[j].y, 5, 0,2*Math.PI);
						}
						let midx=shapes[i].poly[jm2].x + 0.5* (shapes[i].poly[jp2].x-shapes[i].poly[jm2].x);
						let midy=shapes[i].poly[jm2].y + 0.5* (shapes[i].poly[jp2].y-shapes[i].poly[jm2].y);
						let dffx=shapes[i].poly[j].x - midx;
						let dffy=shapes[i].poly[j].y - midy;
						ctx.moveTo(shapes[i].poly[j].x, shapes[i].poly[j].y);
						if (draw_circles) {
							ctx.lineTo(shapes[i].poly[j].x+dffx, shapes[i].poly[j].y+dffy);
						}
						shapes[i].poly[j].arrow_extended = {x:shapes[i].poly[j].x+dffx, y:shapes[i].poly[j].y+dffy};
						//let p0 = xlate(ctx, x, y, 0, svg_xmax, 0, svg_ymax, shapes[i]);
						//ctx.arc(p0[0], p0[1], 5, 0,2*Math.PI);
						//console.log(str_angle);
						shapes[i].poly[j].angle = shapes[i].angles[j];
						shapes[i].is_connector = true;
						add_path_connections(i, 'rect', shapes[i].poly[j], shapes[i].poly[j].arrow_extended.x, shapes[i].poly[j].arrow_extended.y);
						add_path_connections(i, 'path', shapes[i].poly[j], shapes[i].poly[j].x, shapes[i].poly[j].y);
						ctx.stroke();
						ctx.closePath();
						if (shapes[i].id == lkfor_id) {
							console.log(sprintf("jj[%d] angle= %.3f", j, angle));
						}
					} else {
						if ( 
								shapes[i].angles[j] == 90.0 && shapes[i].angles[jp1] == 90 &&
								shapes[i].poly[j].m1_len > shapes[i].poly[j].p1_len &&
								shapes[i].poly[j].m1_len > 1.0 &&
								shapes[i].poly[j].p1_len > 1.0 &&
								shapes[i].poly[jp1].p1_len > shapes[i].poly[jp1].m1_len) {
							ctx.strokeStyle = 'black';
							ctx.beginPath();
							let x = shapes[i].poly[j].x + 0.5 * (shapes[i].poly[jp1].x - shapes[i].poly[j].x);
							let y = shapes[i].poly[j].y + 0.5 * (shapes[i].poly[jp1].y - shapes[i].poly[j].y);
							let dist = 5;
							if (draw_circles) {
								ctx.arc(x, y, dist, 0,2*Math.PI);
							}
							shapes[i].poly[j].end_pt = {x, y};
							shapes[i].poly[j].angle = shapes[i].angles[j];
							ctx.moveTo(x, y);
							let norm_angle = Math.atan2(y - shapes[i].poly[j].y, x - shapes[i].poly[j].x);
							// Draw a normal to the line above
							let x1 = Math.sin(norm_angle) * dist + x;
							let y1 = -Math.cos(norm_angle) * dist + y;
							if (!isPointInPoly(shapes[i].poly, {x1, y1})) {
								x1 = -Math.sin(norm_angle) * dist + x;
								y1 = +Math.cos(norm_angle) * dist + y;
							}
							if (draw_circles) {
								ctx.lineTo(x1, y1);
							}
							shapes[i].poly[j].endpoint_line = {x0:x, y0:y, x1:x1, y1:y1};
							shapes[i].is_connector = true;
							ctx.stroke();
							ctx.closePath();
							add_path_connections(i, 'rect', shapes[i].poly[j], shapes[i].poly[j].endpoint_line.x1, shapes[i].poly[j].endpoint_line.y1);
							add_path_connections(i, 'path', shapes[i].poly[j], shapes[i].poly[j].endpoint_line.x0, shapes[i].poly[j].endpoint_line.y0);
							if (false && shapes[i].id == lkfor_id) {
								console.log(sprintf("jk[%d] angle= %.3f len:jm1= %.3f, jp1= %.3f, jp1p1= %.3f, jp1m1= %.3f",
									j, angle, shapes[i].poly[j].m1_len, shapes[i].poly[j].p1_len,
									shapes[i].poly[jp1].p1_len, shapes[i].poly[jp1].m1_len));
							}
						}
					}
				}
			}
		}
		// end of drawing
		draw_svg_txt_flds();
	}

	draw_svg([]);

	ck_text_enclosing_boxes();

	let hvr_prv_x = -1, hvr_prv_y = -1, hvr_last_i=-1;

	function ck_text_enclosing_boxes() {
		let ck=0;
		for (let i=0; i < shapes.length; i++) {
			if (shapes[i].typ == 'rect' ||
				shapes[i].typ == 'path') {
				shapes[i].text_arr = [];
			}
		}
		for (let i=0; i < shapes.length; i++) {
			if (shapes[i].typ != 'text') {
				continue;
			}
			let xmid = 0.6 * (shapes[i].box.xe - shapes[i].box.xb);
			let ymid = 0.5 * (shapes[i].box.ye - shapes[i].box.yb);
			let x = shapes[i].box.xb + xmid;
			let y = shapes[i].box.yb + ymid;
			shapes[i].rect_arr = [];
			let arr = find_overlap(x, y);
			for (let j=0; j < arr.length; j++) {
				let idx = arr[j];
				if (idx == i || idx < 0) {
					continue;
				}
				shapes[i].rect_arr.push(idx);
			}
			for (let j=arr.length-1; j >= 0; j--) {
				let idx = arr[j];
				if (idx == i || idx < 0) {
					continue;
				}
				if (shapes[idx].typ == 'path') {
					shapes[idx].text_arr.push(i);
				}
				if (shapes[idx].typ == 'rect') {
					shapes[idx].text_arr.push(i);
				}
				break;
			}
			/*
			if (ck++ < 10) {
				let str = "str["+i+"]= "+shapes[i].box.str+" lst_rct= "+shapes[i].rect_prev;
				console.log(str);
				console.log(arr);
			}
			*/
		}
		return;
	}

	function find_overlap(x, y) {
		let arr = [];
		let got_tbox = false;
		let x_0=-1, y_0=-1;
		if (g_cpu_diagram_flds != null) {
			for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
				if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].tbox === 'undefined') {
					continue;
				}
				let tbox = g_cpu_diagram_flds.cpu_diagram_fields[j].tbox;
				if (((tbox.xb <= x && x <= tbox.xe) ||
					 (tbox.xe <= x && x <= tbox.xb)) &&
					((tbox.yb <= y && y <= tbox.ye) ||
					 (tbox.ye <= y && y <= tbox.yb))) {
					console.log("tbox.txt1= "+ tbox.txt);
					x_0 = x;
					y_0 = y;
					got_tbox = true;
					arr.push(-j-1); 
				}
				//g_cpu_diagram_flds.cpu_diagram_fields[j].tbox = {xb:xb, xe:xe, yb:yb, ye:ye, txt:cp_str};
			}
		}
		for (let i=0; i < shapes.length; i++) {
			if (shapes[i].box != null) {
				if (((shapes[i].box.xb <= x && x <= shapes[i].box.xe) ||
					 (shapes[i].box.xe <= x && x <= shapes[i].box.xb)) &&
					((shapes[i].box.yb <= y && y <= shapes[i].box.ye) ||
					 (shapes[i].box.ye <= y && y <= shapes[i].box.yb))) {
					arr.push(i);
				}
			}
			if (typeof shapes[i].poly != 'undefined') {
				if (isPointInPoly(shapes[i].poly, {x, y})) {
					arr.push(i);
				}
			}
		}
		return arr;
	}

	let did_svg_highlight = false;

	function build_svg_json_file() {
		let str_all = "", cma_all="  ";
		for (let i=0; i < shapes.length; i++) {
			let str = "";
			if ((shapes[i].typ == 'rect') && shapes[i].text_arr.length > 0) {
				str = "{ "+shapes[i].id+" ";
				let use_k = 0;
				let tidx0 = shapes[i].text_arr[0];
				// find biggest text (by font size)
				for (let k=1; k < shapes[i].text_arr.length; k++) {
					let tidx = shapes[i].text_arr[k];
					if (shapes[tidx].nfont_sz > shapes[tidx0].nfont_sz) {
						use_k = k;
					}
				}
				tidx0 = shapes[i].text_arr[use_k];
				for (let k=0; k < shapes[i].text_arr.length; k++) {
					let tidx = shapes[i].text_arr[k];
					if (shapes[tidx].nfont_sz >= shapes[tidx0].nfont_sz &&
						shapes[tidx].box.yb < shapes[tidx0].box.yb) {
							use_k = k;
							tidx0 = shapes[i].text_arr[use_k];
					}
				}
				let str2 = shapes[tidx0].text.trim();
				for (let k=0; k < shapes[i].text_arr.length; k++) {
					let tidx = shapes[i].text_arr[k];
					if (tidx != tidx0) {
						str2 += ", " + shapes[tidx].text.trim();
					}
				}
				let ply_str = "", cma="";
				for (let k=0; k < shapes[i].poly.length; k++) {
					let strx = sprintf('%s{"x":%.3f,"y":%.3f}', cma, shapes[i].poly[k].x, shapes[i].poly[k].y);
					cma = ", ";
					ply_str += strx;
				}
				str_all += sprintf('%s{"id":"%s", "idx":%d, "txt":"%s", "poly":[%s]}\n',
						cma_all, shapes[i].id, i, str2, ply_str);
				cma_all = ", ";

			}
		}
		for (let i=0; i < shapes.length; i++) {
			let str = "";
			if (shapes[i].typ == 'path' &&
				typeof shapes[i].is_connector != 'undefined' &&
				shapes[i].is_connector == true ) {
				//console.log(shapes[i]);
				let ply_str = "", cma="";
				let arr = [];
				for (let j=0; j < shapes[i].poly.length; j++) {
					let con_str = "", con_cma= "";
					if (typeof shapes[i].poly[j].im_connected_to != 'undefined') {
						let arr2 = shapes[i].poly[j].im_connected_to;
						for (let k=0; k < arr2.length; k++) {
							if (arr.indexOf(arr2[k]) == -1) {
								con_str += con_cma + arr2[k];
								con_cma = ", ";
								arr.push(arr2[k]);
							}
						}
					}
					if (con_str != "") {
						con_str = ', "con_to":['+con_str+']';
					}
					let strx = sprintf('%s{"x":%.3f,"y":%.3f%s}', cma, shapes[i].poly[j].x, shapes[i].poly[j].y, con_str);
					cma = ", ";
					ply_str += strx;
				}
				let str2 = "", str2_cma= "";
				for (let k=0; k < shapes[i].text_arr.length; k++) {
					let tidx = shapes[i].text_arr[k];
						str2 += str2_cma + shapes[tidx].text.trim();
						str2_cma = ", ";
				}
				let con_to_me = "", con_to_me_cma= "";
				if (typeof shapes[i].is_connected_to_me != 'undefined') {
					for (let m=0; m < shapes[i].is_connected_to_me.length; m++) {
						let cf = shapes[i].is_connected_to_me[m];
						if (
							shapes[cf].typ == 'path' &&
							typeof shapes[cf].is_connector != 'undefined' &&
							shapes[cf].is_connector == true &&
							arr.indexOf(cf) == -1) {
							arr.push(cf);
							con_to_me += con_to_me_cma + cf;
							con_to_me_cma = ", ";
							//arr = follow_paths(cf, arr);
						}
					}
					if (con_to_me != "") {
						con_to_me = ', "con_to_me":['+con_to_me+']';
					}
				}
				//str2 = str2.replace("Âµ", "µ");
				//str2 = str2.replace("&amp;", "&");
				//str2 = str2.replace(/Âµ/g, "µ");
				str2 = str2.replace(/&amp;/g, "&");
				str_all += sprintf('%s{"id":"%s", "idx":%d, "txt":"%s", "poly":[%s]%s}\n',
						cma_all, shapes[i].id, i, str2, ply_str, con_to_me);
				cma_all = ", ";
			}
			if (false && (shapes[i].typ == 'rect') && shapes[i].text_arr.length > 0) {
				let use_k = 0;
				let tidx0 = shapes[i].text_arr[0];
				// find biggest text (by font size)
				for (let k=1; k < shapes[i].text_arr.length; k++) {
					let tidx = shapes[i].text_arr[k];
					if (shapes[tidx].nfont_sz > shapes[tidx0].nfont_sz) {
						use_k = k;
					}
				}
				tidx0 = shapes[i].text_arr[use_k];
				for (let k=0; k < shapes[i].text_arr.length; k++) {
					let tidx = shapes[i].text_arr[k];
					if (shapes[tidx].nfont_sz >= shapes[tidx0].nfont_sz &&
						shapes[tidx].box.yb < shapes[tidx0].box.yb) {
							use_k = k;
							tidx0 = shapes[i].text_arr[use_k];
					}
				}
				let str2 = shapes[tidx0].text.trim();
				for (let k=0; k < shapes[i].text_arr.length; k++) {
					let tidx = shapes[i].text_arr[k];
					if (tidx != tidx0) {
						str2 += ", " + shapes[tidx].text.trim();
					}
				}
				//str2 = str2.replace(/Âµ/g, "µ");
				str2 = str2.replace(/&amp;/g, "&");
				let ply_str = "", cma="";
				for (let k=0; k < shapes[i].poly.length; k++) {
					let strx = sprintf('%s{"x":%.3f,"y":%.3f}', cma, shapes[i].poly[k].x, shapes[i].y);
					cma = ", ";
					ply_str += strx;
				}
				str_all += sprintf('%s{"id":"%s", "idx":%d, "txt":"%s", "poly":[%s]}\n',
						cma_all, shapes[i].id, i, str2, ply_str);
				cma_all = ", ";

			}
		}
		str_all = '{"svg":{"max_x":'+px_wide+',"max_y":'+px_high+', "shapes":['+str_all+']}}';
		return {status:1, str:str_all};
	}

	g_svg_obj = build_svg_json_file();

	function draw_svg_txt_flds()
	{
		if (g_cpu_diagram_flds == null) {
			return;
		}
		let txt_tbl = [];
		for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr === 'undefined') {
				continue;
			}
			let cp_str = "";
			let val_arr = [];
			for (let ii=0; ii < g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length; ii++) {
				let str = sprintf(g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_fmt,
					g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr[ii]);
				val_arr.push(str);
				cp_str += (ii > 0 ? " ":"") + str;
			}
			let text_align = 'left';
			let text_anchor = 'bottom';
			let nfont_sz = 11;
			let font_sz = "11px";
			nfont_sz *= scale_ratio;
			let fmxx = g_cpu_diagram_flds.cpu_diagram_hdr.max_x;
			let fmxy = g_cpu_diagram_flds.cpu_diagram_hdr.max_y;
			let x0 = svg_xmax * g_cpu_diagram_flds.cpu_diagram_fields[j].fld.x/fmxx;
			let y0 = svg_ymax * g_cpu_diagram_flds.cpu_diagram_fields[j].fld.y/fmxy;
			let p0 = xlate(ctx, x0, y0, 0, svg_xmax, 0, svg_ymax, null);
			let rot = 0;
			let angle = 0.0;
			ctx.beginPath();
			ctx.textAlign = text_align;
			ctx.textBaseline = text_anchor;
			ctx.fillStyle = 'black';
			//ctx.font = font_sz + ' Arial';
			ctx.font = sprintf("%.3fpx sans-serif", nfont_sz);
			//ctx.font = font_sz + ' sans-serif';
			let str = cp_str;
			if (str.indexOf('&amp;') >= 0) {
				str = str.replace('&amp;', '&');
			}
			let twidth = ctx.measureText(str).width;
			let xb, xe, yb, ye;
			if (text_align == 'start' || text_align == 'left') {
				xb = p0[0];
				xe = xb + twidth;
			} else if (text_align == 'end' || text_align == 'right') {
				xe = p0[0];
				xb = xe - twidth;
			} else if (text_align == 'center') {
				xb = p0[0] - 0.5*twidth;
				xe = p0[0] + 0.5*twidth;
			} else {
				console.log("unknown text_align= "+text_align);
				xb = p0[0];
				xe = xb + twidth;
			}
			if (text_anchor == 'top' || text_anchor == 'hanging') {
				yb = p0[1];
				ye = yb - nfont_sz;
			} else if (text_anchor == 'bottom' || text_anchor == 'alphabetic') {
				ye = p0[1];
				yb = ye - nfont_sz;
			} else if (text_anchor == 'middle') {
				yb = p0[1] - 0.5*nfont_sz;
				ye = p0[1] + 0.5*nfont_sz;
			} else {
				console.log("unknown text_anchor= "+text_anchor);
				ye = p0[1];
				yb = ye - nfont_sz;
			}
			//console.log(sprintf("txt %s %s", text_align, text_anchor));
			//console.log(shapes[i].box);

			ctx.fillText(str, p0[0], p0[1]);
			g_cpu_diagram_flds.cpu_diagram_fields[j].tbox = {xb:xb, xe:xe, yb:yb, ye:ye, txt:cp_str};
			let iidx = txt_tbl.length;
			txt_tbl.push({jidx:j, iidx:iidx, txt:[]});
			for (let k=0; k < val_arr.length; k++) {
				txt_tbl[iidx].txt.push(val_arr[k]);
			}
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].rng !== 'undefined') {
				let off = 0.0;
				let hi = 0.4 * (ye - yb);
				for (let ii=0; ii < g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length; ii++) {
				ctx.beginPath();
				ctx.arc(xe+2+off+hi, yb+hi, hi, 0, 2 * Math.PI, false);
				if (g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr[0] <=
					g_cpu_diagram_flds.cpu_diagram_fields[j].rng.green) {
					ctx.fillStyle = 'green';
				} else if (g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr[0] <=
					g_cpu_diagram_flds.cpu_diagram_fields[j].rng.yellow) {
					ctx.fillStyle = 'yellow';
				} else {
					ctx.fillStyle = 'red';
				}
				off += 2.0*hi;
				ctx.fill();
				ctx.lineWidth = 1;
				ctx.strokeStyle = 'black';
				ctx.stroke();
				}
			}
		}
		txt_tbl.sort(function(a, b){
			let lh = g_cpu_diagram_flds.cpu_diagram_fields[a.jidx].y_label;
			let rh = g_cpu_diagram_flds.cpu_diagram_fields[b.jidx].y_label;
			if(lh < rh) { return -1; }
			if(lh > rh) { return 1; }
			return 0;
		})
		let tbl_str = "<table border='1' style='float: left'>";
		for (let i=0; i < txt_tbl.length; i++) {
			let j = txt_tbl[i].jidx;
			let desc = g_cpu_diagram_flds.cpu_diagram_fields[j].desc;
			let ct   = g_cpu_diagram_flds.cpu_diagram_fields[j].chart_tag;
			tbl_str += "<tr>";
			tbl_str += "<td title='"+desc+"\nchart_tag:"+ct+"'>" +g_cpu_diagram_flds.cpu_diagram_fields[j].y_label + "</td>";
			if (typeof txt_tbl[i] != 'undefined') {
			for (let k=0; k < txt_tbl[i].txt.length; k++) {
				let txt = txt_tbl[i].txt[k];
				tbl_str += "<td align='right' title='"+ desc + "'>" + txt + "</td>";
			}
			}
			tbl_str += "</tr>";
		}
		tbl_str += "</table>";
		let tbl_str2 = parse_resource_stalls(txt_tbl);
		//mytable.innerHTML = tbl_str2 + tbl_str;
		mytable.innerHTML = tbl_str2;
	}

	function lkup_chrt(lkup, txt_tbl, chrt) {
		if (typeof lkup[chrt] != 'undefined') {
			let i = lkup[chrt];
			let j = txt_tbl[i].jidx;
			return {j:txt_tbl[i].jidx, i:i, desc:g_cpu_diagram_flds.cpu_diagram_fields[j].desc,
				ylab:g_cpu_diagram_flds.cpu_diagram_fields[j].y_label,
				vals:g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr,
				rng: g_cpu_diagram_flds.cpu_diagram_fields[j].rng,
				tvals:txt_tbl[i].txt};
		}
		return {j:-1, i:-1};
	}
	function lkup_fillin(ret, flds_max, resource, prfx) {
		let stall = "";
		let txt     = "";
		let t = "";
		let prefix = "";
		if (typeof prfx != "undefined") {
			for (let i=0; i < prfx; i++) {
				prefix += "&nbsp;";
			}
		}
		for (let i=0; i < flds_max; i++) {
			let str = "";
			let cir = ""; 
			if (i < ret.vals.length) {
				str = ret.tvals[i];
				if (ret.vals[i] <= ret.rng.green) {
					cir = "<span class='dot_green'>n</span>";
				} else if (ret.vals[i] <= ret.rng.yellow) {
					cir = "<span class='dot_yellow'>?</span>";
				} else {
					cir = "<span class='dot_red'>y</span>";
				}
			}
			stall += "<td title='"+ret.desc+"'>"+cir+"</td>";
			txt += "<td align='right' title='"+ret.desc+"'><span class='nowrap'>"+str+ "</span></td>";
		}
		t += "<tr>";
		t += "<td title='"+ret.desc+"'>" +prefix+ret.ylab + "</td>";
		t += txt;
		t += stall;
		t += "<td title='"+ret.desc+"'>" +resource + "</td>";
		t += "</tr>";
		return t;
	}

	function parse_resource_stalls(txt_tbl) {
		if (svg_name != "haswell_block_diagram.svg") {
			return "";
		}
		let lkup = {};
		let flds_max = 0;
		for (let i=0; i < txt_tbl.length; i++) {
			let j  = txt_tbl[i].jidx;
			let ct = g_cpu_diagram_flds.cpu_diagram_fields[j].chart_tag;
			lkup[ct] = i;
			let mx = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length;
			if (flds_max < mx) {
				flds_max = mx;
			}
		}
		let t = "<table border='1' style='float: left'>";
		t += "<tr><td>Haswell CPU diagram metric</td>";
		for (let i=0; i < flds_max; i++) {
			t += "<td title='Value of the computed metric for core or system'>core"+i+" val</td>";
		}
		for (let i=0; i < flds_max; i++) {
			t += "<td title='Some of these are guesses as to whether the core is stalled.'>core"+i+" stalled</td>";
		}
		t += "<td>Resource utilization description</td></tr>";
		t += "<tr><td>Memory Subsystem </td></tr>";
		let ret = lkup_chrt(lkup, txt_tbl, "UNC_HASWELL_PEAK_BW_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "DRAM: pct of max possible memory BW 25.9 GB/s", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "L3_MISS_BYTES_PER_CYCLE_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "L3_misses: max L3 misses about 10 bytes/cycle", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "SQ_FULL_PER_CYCLE_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "L2_misses: pct of cycles SuperQueue full so can't take more requests", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "L2_MISS_BYTES_PER_CYCLE_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "L2_misses: path to L3 64 bytes/cycle max", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "L2_TRANS_ALL_PER_CYCLE_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "L2_traffic: path L1d to L2 64 bytes/cycle max", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "ICACHE_MISS_BYTES_PER_CYCLE_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "L2_traffic: path icache to L2 64 bytes/cycle max", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "L1D_PENDING_MISS_FB_FULL_PER_TOT_CYCLES_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "FB stalls: pct cycles L1d demand request blocked due to Fill Buffer (FB) full", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "L1D_REPL_BYTES_PER_CYCLE_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "L1d: L1d to load buffer: 2 paths of 32 bytes/cycle", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "RESOURCE_STALLS.SB_PER_TOT_CYCLES_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "L1d: L1d to store buffer: %cycles stalled due store buffer full", 2);
		}
		t += "<tr><td>Front End</td></tr>";
		ret = lkup_chrt(lkup, txt_tbl, "IFETCH_STALLS_PER_TOT_CYCLES_CHART", 2);
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "L1i: %cycles stalled due to L1 icache miss", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "MISPRED_CYCLES_PER_TOT_CYCLES_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "Branch prediction Unit: %cycles allocator stalled due to mispredict or memory nuke", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "CYCLES_PER_DSB_UOPS_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "Decode Stream Buffer: cycles per uop to Instr Decode Queue IDQ from DSB path. Min possible is 0.25 cycles/uop", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "CYCLES_PER_IDQ_ALL_UOPS_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "IDQ: cycles per uop to IDQ from MITE path. Min possible is 0.25 cycles/uop", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "CYCLES_PER_LSD_UOPS_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "LSD: cycles per Loop Stream Detector uop to IDQ. Min possible is 0.25 cycles/uop. ", 2);
		}
		t += "<tr><td>Execution Engine</td></tr>";
		ret = lkup_chrt(lkup, txt_tbl, "UOPS_ISSUED.CORE_STALL_CYCLES_PER_TOT_CYCLES_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "RAT: pct of total cycles where uops not issued by Register Allocation Table", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "UOPS_RETIRED.CORE_STALL_CYCLES_PER_TOT_CYCLES_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "EXE: pct of total cycles where uops not retired", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "RESOURCE_STALLS.RS_PER_TOT_CYCLES_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "RS: pct of total cycles stalled due to no eligible Reservation Station entry available", 2);
		}
		ret = lkup_chrt(lkup, txt_tbl, "CYCLES_PER_RAT_UOPS_CHART");
		if (ret.i > -1) {
			t += lkup_fillin(ret, flds_max, "RAT: cycles per RAT uop issued. Min possible is 0.25 cycles/uop. Compare to the DSB, LSD, IDQ cycles/uop to see which path is probably mostly feeding the RAT", 2);
		}
		for (let i=0; i < 8; i++) {
			ret = lkup_chrt(lkup, txt_tbl, "CYCLES_PER_UOP_port_"+i+"_CHART");
			if (ret.i > -1) {
				t += lkup_fillin(ret, flds_max, "port"+i+": cycles/uop on port"+i+". Big values may mean port is not used much", 4);
			}
		}
		t += "</table>";
		return t;
	}

	mycanvas.onmousemove = function(e) {
		// important: correct mouse position:
		let rect = this.getBoundingClientRect(),
			x = e.clientX - rect.left,
			y = e.clientY - rect.top;
		if (x == hvr_prv_x && y == hvr_prv_y) {
			return;
		}
		let current_tooltip_text = "aa";
		hvr_prv_x = x;
		hvr_prv_y = y;
		let arr = find_overlap(x, y);
		if (arr.length == 0) {
			//clearToolTipText(mytooltip);
			current_tooltip_text = "";
			tt_prv_x = -2;
			tt_prv_y = -2;
			mycanvas.title = "";
			//current_tooltip_shape = -1;
		}
		let got_neg = false;
		for (let j=0; j < arr.length; j++) {
			let i = arr[j];
			if (i < 0) {
				got_neg = true;
				break;
			}
		}
		if (!got_neg) {
			mycanvas.title = "";
		}
		for (let j=0; j < arr.length; j++) {
			let i = arr[j];
			if (i < 0) {
				let ui= -i-1;
				let ts= g_cpu_diagram_flds.cpu_diagram_fields[ui].y_label+ ": " +
					g_cpu_diagram_flds.cpu_diagram_fields[ui].tbox.txt+',\n'+
					g_cpu_diagram_flds.cpu_diagram_fields[ui].desc;
				console.log(ts);
				if (tt_prv_x != x || tt_prv_y != y) {
					tt_prv_x = x;
					tt_prv_y = y;
					let pgx = e.pageX, pgy = e.pagey;
				//console.log(sprintf("tooltip txt= %s, x= %.3f, y= %.3f, ttl= %s, ttr=%s ttt= %s ttb= %s",ts, x, y,
				//mytooltip.style.left, mytooltip.style.right, mytooltip.style.top, mytooltip.style.bottom));
					mycanvas.title = ts;
					//console.log(mytooltip);
				}
				continue;
			}
			let str = "";
			if (shapes[i].typ == 'path') {
				str = "pth= "+i+",id= "+shapes[i].id;
				//console.log(shapes[i]);
			}
			if ((shapes[i].typ == 'rect' || shapes[i].typ == 'path') && shapes[i].text_arr.length > 0) {
				str = "str= "+shapes[i].box.str+" lst_rct= "+shapes[i].rect_prev + ", id= "+shapes[i].id;
				let use_k = 0;
				let tidx0 = shapes[i].text_arr[0];
				// find biggest text (by font size)
				for (let k=1; k < shapes[i].text_arr.length; k++) {
					let tidx = shapes[i].text_arr[k];
					if (shapes[tidx].nfont_sz > shapes[tidx0].nfont_sz) {
						use_k = k;
					}
				}
				tidx0 = shapes[i].text_arr[use_k];
				for (let k=0; k < shapes[i].text_arr.length; k++) {
					let tidx = shapes[i].text_arr[k];
					if (shapes[tidx].nfont_sz >= shapes[tidx0].nfont_sz &&
						shapes[tidx].box.yb < shapes[tidx0].box.yb) {
							use_k = k;
							tidx0 = shapes[i].text_arr[use_k];
					}
				}
				if (true && i == 223) {
					for (let k=0; k < shapes[i].text_arr.length; k++) {
					let tidx = shapes[i].text_arr[k];
					console.log(sprintf("rect %d: txt %d str= %s, fntsz= %f, yb= %f, shape:",
								i, k, shapes[tidx].box.str, shapes[tidx].nfont_sz, shapes[tidx].box.yb));
					}
					console.log(shapes[i]);
				}
				let idx = shapes[i].text_arr[use_k];
				str += ", str0= "+shapes[idx].box.str;
			}
			if (hvr_last_i != i) {
				console.log(str);
				hvr_last_i = i;
			}
		}
		function follow_paths(i, arr) {
			if (shapes[i].typ == 'path' &&
				typeof shapes[i].is_connector != 'undefined' &&
				shapes[i].is_connector == true ) {
				//console.log(shapes[i]);
				for (let j=0; j < shapes[i].poly.length; j++) {
					if (typeof shapes[i].poly[j].im_connected_to != 'undefined') {
						let arr2 = shapes[i].poly[j].im_connected_to;
						for (let k=0; k < arr2.length; k++) {
							if (arr.indexOf(arr2[k]) == -1) {
								arr.push(arr2[k]);
							}
						}
					}
				}
				if (typeof shapes[i].is_connected_to_me != 'undefined') {
					for (let m=0; m < shapes[i].is_connected_to_me.length; m++) {
						let cf = shapes[i].is_connected_to_me[m];
						if (
							shapes[cf].typ == 'path' &&
							typeof shapes[cf].is_connector != 'undefined' &&
							shapes[cf].is_connector == true &&
							arr.indexOf(cf) == -1) {
							arr.push(cf);
							//arr = follow_paths(cf, arr);
						}
				 	}
				}
			}
			return arr;
		}

		for (let m=0; m < arr.length; m++) {
			let i = arr[m];
			if (i < 0) {
				// svg cpu_diagram tbox with avg values from box
				continue;
			}
			let str = "";
			arr = follow_paths(i, arr);
		}
		if (arr.length > 0) {
			draw_svg(arr);
			did_svg_highlight = true;
		} else {
			if (did_svg_highlight) {
				draw_svg([]);
				did_svg_highlight = false;
			}
		}
		return;
	};

	function isPointInPoly(poly, pt){
	//Copyright
	// We authorize the copy and modification of all the codes on the site, since the original author credits are kept
	//+ Jonas Raoni Soares Silva
	//@ http://jsfromhell.com/math/is-point-in-poly [rev. #0]
		let c = false;
		for(let i = -1, l = poly.length, j = l - 1; ++i < l; j = i)
			((poly[i].y <= pt.y && pt.y < poly[j].y) || (poly[j].y <= pt.y && pt.y < poly[i].y))
			&& (pt.x < (poly[j].x - poly[i].x) * (pt.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
			&& (c = !c);
		return c;
	}

	draw_svg([]);
	let zoom_ckr = new TaskTimer(1000);

	zoom_ckr.addTask({
	    name: 'zoom_ck '+hvr_clr,       // unique name of the task
	    tickInterval: 2,    // run every 5 ticks (5 x interval = 5000 ms)
	    totalRuns: 0,      // run 10 times only. (set to 0 for unlimited times)
	    zoom_iter_last: 0,      // run 10 times only. (set to 0 for unlimited times)
	    callback: function (task) {
		// code to be executed on each run
		//if (gsync_zoom_linked && !gsync_zoom_active_now)
			if (gsync_zoom_redrawn_charts.cpu_diag_redraw > task.zoom_iter_last) {
				//console.log('task :'+task.name + ' id= '+task.zoom_iter_last);
				task.zoom_iter_last = gsync_zoom_redrawn_charts.cpu_diag_redraw;
				console.log("/// now redraw cpu_diagram");
				draw_svg([]);
			}
	    }
	});

	// Start the timer
	zoom_ckr.start();

	console.log("got to end of parse_svg()");
}
