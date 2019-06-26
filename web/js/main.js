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
var g_charts_done = {cntr:0, typ:""};
var g_cpu_diagram_flds = null;
var g_cpu_diagram_canvas = null;
var g_cpu_diagram_canvas_ctx = null;
var g_cpu_diagram_draw_svg = null;
var g_svg_scale_ratio = null;
const CH_TYPE_LINE="line";
const CH_TYPE_BLOCK="block";
const CH_TYPE_STACKED="stacked";
var g_got_cpu_diagram_svg = false;
var gmsg_span = null;
var gpixels_high_default = 250;
var g_tot_line_divisions = {max:100};
var doc_title_def = "OPPAT: Open Power/Performance Analysis Tool";
var gjson = {};
var gjson_str_pool = null;
var g_BigEval = new BigEval();
var did_c10_colors = 0;
var g_d3_clrs_c10 = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"];
var g_d3_clrs_c20 = ["#1f77b4", "#aec7e8", "#ff7f0e", "#ffbb78", "#2ca02c", "#98df8a", "#d62728", "#ff9896", "#9467bd", "#c5b0d5",
  "#8c564b", "#c49c94", "#e377c2", "#f7b6d2", "#7f7f7f", "#c7c7c7", "#bcbd22", "#dbdb8d", "#17becf", "#9edae5" ];
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
	//console.log("did setToolTip");
}

function ck_def_notnull(ele)
{
	if (typeof ele !== 'undefined' && ele !== null) {
		return true;
	}
	return false;
}

// Translated from code from Sam Kimbrel and Charlie Loyd
// via http://basecase.org/env/on-rainbows
const PHI = (1 + Math.pow(5, 0.5)) / 2;
function color_sinebow(hue)
{
    // Choose a color from a sin^2-softened rainbow.
    let red = Math.pow(Math.cos(Math.PI * hue), 2) * 255;
    let green = Math.pow(Math.cos(Math.PI * (hue + 1/3)), 2) * 255;
    let blue = Math.pow(Math.cos(Math.PI * (hue + 2/3)), 2) * 255;
    return [Math.floor(red), Math.floor(green), Math.floor(blue)];
}
function color_choose_hex_sinebow(n)
{
	// start getting dupe colors for n > 256 (well... at n=512 get a few dupes).
    // Choose color number `n` from the sinebow using the angle of phi to reduce
    //  collision possibilities, and returning it as a hex-formatted color.
    let color = color_sinebow(n * PHI);
    return "#" + color.map(function (channel) {
        let hex = channel.toString(16);
        if (channel < 16) {
            hex = '0' + hex;
        }
        return hex;
    }).join('');
}

function color_ck_dupes(mx)
{
	let use_color_list = [];
	for (let i=0; i < mx; i++) {
		use_color_list.push(color_choose_hex_sinebow(i));
	}
	console.log("__beg clr dupe clrs= "+mx);
	for (let i=0; i < mx; i++) {
		for (let j=0; j < i; j++) {
			if (use_color_list[i] == use_color_list[j]) {
				console.log(sprintf("__clr dupe %d %d", i, j));
			}
		}
	}
	console.log("__end clr dupe clrs= "+mx);
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
	if (gmsg_span === null) {
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
var gsync_zoom_redrawn_charts = {cntr:0, cpu_diag_redrawn:0, cpu_diag_redraw_requests:0, typ:""};
var gsync_zoom_redrawn_charts_map = [];
var gsync_zoom_active_now = false;
var gsync_zoom_active_redraw_beg_tm = 0;
var gsync_zoom_charts_redrawn = 0;
var gsync_zoom_charts_hash = {}
var gsync_zoom_arr = [];
var gsync_zoom_last_zoom = {chrt_idx:-1, x0:0.0, x1:0.0, abs_x0:0.0, abs_x1:0.0};
var gsync_text = "Zoom/Pan: Zoom all to last";
var gLinkZoom_iter = 0;

function got_all_OS_view_images(whch_txt, image_whch_txt_init)
{
	// return true if got all the images or don't have any images
	// else return false
	if (g_cpu_diagram_flds === null) {
		return true;
	}
	let need_imgs = 0;
	let ready_imgs = 0;
	for (let j=0; j < gjson.chart_data.length; j++) {
		if (typeof gjson.chart_data[j].fl_image_ready !== 'undefined') {
			need_imgs++;
			if (whch_txt < 0) {
				ready_imgs++;
			} else {
				if (typeof gjson.chart_data[j].image_whch_txt !== 'undefined' &&
						gjson.chart_data[j].image_whch_txt != image_whch_txt_init) {
					ready_imgs++;
				}
			}
		}
		if (typeof gjson.chart_data[j].image_ready !== 'undefined') {
			need_imgs++;
			if (whch_txt < 0) {
				ready_imgs++;
			} else {
				if (typeof gjson.chart_data[j].image_whch_txt !== 'undefined' &&
						gjson.chart_data[j].image_whch_txt != image_whch_txt_init) {
					ready_imgs++;
				}
			}
		}
	}
	console.log(sprintf("by_phase: ck_img: whch_txt= %d, need= %d, rdy= %d", whch_txt, need_imgs, ready_imgs));
	if (ready_imgs == need_imgs) {
		return true;
	}
	return false;
}

function set_zoom_all_charts(j, need_tag, po_lp, from_where)
{
	//let j = gsync_zoom_last_zoom.chrt_idx;
	//let need_tag = gjson.chart_data[j].file_tag
	let j_sv = j;
	gLinkZoom_iter++;
	if (j_sv >= 0) {
		console.log(sprintf("lnk_zm[%d]: x0= %f x1= %f need_tag= %s", j_sv, gcanvas_args[j_sv][6], gcanvas_args[j_sv][7], need_tag));
	}
	let ft = gjson.chart_data[j_sv].file_tag;
	let ct = gjson.chart_data[j_sv].chart_tag;
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
		if (tabs_x0 != gsync_zoom_last_zoom.abs_x0) {
			tabs_x0 = gsync_zoom_last_zoom.abs_x0;
		}
		if (tabs_x1 != gsync_zoom_last_zoom.abs_x1) {
			tabs_x1 = gsync_zoom_last_zoom.abs_x1;
		}
		gsync_zoom_arr[i].x0 = tabs_x0;
		gsync_zoom_arr[i].x1 = tabs_x1;
		gsync_zoom_arr[i].iter = gLinkZoom_iter;
		//console.log("Zooom ii= "+i);
	}
	gsync_zoom_redrawn_charts.cntr = 0;
	gsync_zoom_redrawn_charts.need_to_redraw = need_to_redraw;
	let tm_n00 = performance.now();
	let tm_arr = [];
	tm_arr.length = gjson.chart_data.length;
	tm_arr.fill(0.0);
	let cntr = 0;
	for (let j=0; j < 2; j++) {
		// first (j==0) do charts that need to generate images
		// then  (j==1) do charts that don't need to generate images
		// Not really sure if this will make a difference in how long it takes to redraw all
		for (let i=0; i < gjson.chart_data.length; i++) {
			if (gjson.chart_data[i].file_tag != need_tag) {
				continue;
			}
			if (i == j_sv) {
				continue;
			}
			if ((j == 0 && typeof gjson.chart_data[i].fl_image_ready !== 'undefined') ||
				(j == 1 && typeof gjson.chart_data[i].fl_image_ready === 'undefined')) {
				if (from_where == "LinkZoom") {
					let tm_n = performance.now();
					let txt = sprintf("draw chrt %d of %d, elap_tm= %.2f secs",
							++cntr, gjson.chart_data.length, 0.001 * (tm_n-tm_n00));
					mymodal_span_text.innerHTML = txt;
					update_status(txt);
				}
				gjson.chart_data[i].zoom_func_obj.task.do_zoom = true;
				gjson.chart_data[i].zoom_func_obj.task.need_to_redraw = need_to_redraw;
				let tm_0 = performance.now();
				gjson.chart_data[i].zoom_func_obj.func(gjson.chart_data[i].zoom_func_obj.task);
				let tm_1 = performance.now();
				tm_arr[i] += tm_1 - tm_0;
			}
		}
	}
	let tm_n01 = performance.now();
	if (g_cpu_diagram_draw_svg !== null) {
		let jj= 0, jj_max = 50, image_whch_txt_init=-200;
		console.log("call g_cpu_diagram_draw_svg beg");
		function myDelay () {           //  create a loop function
			setTimeout(function () {    //  call a 3s setTimeout when the loop is called
				console.log(sprintf('wait_for_imgs jj= %d', jj));
				jj++;                     //  increment the counter
				if (!got_all_OS_view_images(1, image_whch_txt_init)) {
					myDelay();             //  ..  again which will trigger another 
				} else {
					console.log("__mem: call g_cpu_diagram_draw_svg");
					// this call is done later if now LinkZoom
					if (from_where == "LinkZoom") {
						g_cpu_diagram_draw_svg([], -1, -1);
					}
				}
			}, 500);
		}
		//if (!got_all_OS_view_images(1, image_whch_txt_init)) {
			myDelay();
		//}
	}
	let tm_n02 = performance.now();
	if (ct == "SYSCALL_OUTSTANDING_CHART") {
		let totl = 0.0;
		if (false) {
		tm_arr.sort(function(a, b){return b - a});
		let tp = (tm_arr.length > 10 ? 10 : tm_arr.length);
		for (let i=0; i < tp; i++) {
			console.log(sprintf("__mem: top tms[%d] tm= %.2f ttl= %s", i, tm_arr[i], 
				gjson.chart_data[i].title));
		}
		for (let i=0; i < tm_arr.length; i++) {
			totl += tm_arr[i];
		}
		}
		console.log(sprintf("__mem: set_zoom_all_charts: tm_tot= %.2f, pt1= %.2f, pt2= %.2f, from= %s tm_arr_tot= %.2f",
					tm_n02-tm_n00, tm_n01-tm_n00, tm_n02-tm_n01, from_where, totl));

	}
	if (from_where == "LinkZoom") {
		let tm_n = performance.now();
		let txt = sprintf("finished zoom all of %d charts. time to zoom all: %.2f secs. ", cntr, 0.001 * (tm_n-tm_n00));
		update_status(txt);
		setTimeout(function () {
			mymodal.style.display = "none";
			document.title = doc_title_def;
    	}, 2000);
	}
	gsync_zoom_linked = true;
}

function LinkZoom( el, cb )
{
	let ckd = cb.checked;
	console.log("zoom_ck= "+ckd);
	if (el.textContent === gsync_text) {
		$('#lhs_menu').BootSideMenu.close();
		console.log(sprintf("lnk_zm: x0= %f x1= %f", gsync_zoom_last_zoom.x0, gsync_zoom_last_zoom.x1));

		let j = gsync_zoom_last_zoom.chrt_idx;
		//abcd
		let txt = "doing zoom all";
		document.title = txt;
		mymodal.style.display = "none";
		mymodal_span_text.innerHTML = document.title;
		update_status(txt);
		mymodal.style.display = "block";
		console.log(txt);
		setTimeout(function () {
			set_zoom_all_charts(gsync_zoom_last_zoom.chrt_idx, gjson.chart_data[j].file_tag, -1, "LinkZoom");
    	}, 500);

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
	if (ele !== null) {
		//console.log("try to scroll to "+ele_nm);
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
	if (ele !== null) {
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

function get_chart_options(chart_options)
{
	let ch_options = {
		overlapping_ranges_within_area:false,
		tot_line_add_values_in_interval:false,
		tot_line_legend_weight_by_dura:false,
		tot_line_legend_weight_by_x_by_y:false,
		tot_line_bucket_by_end_of_sample:false,
		sum_to_interval:false,
		show_even_if_all_zero:false,
	}
	if (typeof chart_options !== 'undefined') {
		let tst_opt = "OVERLAPPING_RANGES_WITHIN_AREA";
		if (chart_options.indexOf(tst_opt) >= 0) {
			ch_options.overlapping_ranges_within_area = true;
		}
		tst_opt = "TOT_LINE_ADD_VALUES_IN_INTERVAL";
		if (chart_options.indexOf(tst_opt) >= 0) {
			ch_options.tot_line_add_values_in_interval = true;
		}
		tst_opt = "TOT_LINE_LEGEND_WEIGHT_BY_DURA";
		if (chart_options.indexOf(tst_opt) >= 0) {
			ch_options.tot_line_legend_weight_by_dura = true;
		}
		tst_opt = "TOT_LINE_LEGEND_WEIGHT_BY_X_BY_Y";
		if (chart_options.indexOf(tst_opt) >= 0) {
			ch_options.tot_line_legend_weight_by_x_by_y = true;
		}
		tst_opt = "TOT_LINE_BUCKET_BY_END_OF_SAMPLE";
		if (chart_options.indexOf(tst_opt) >= 0) {
			ch_options.tot_line_bucket_by_end_of_sample = true;
		}
		tst_opt = "SHOW_EVEN_IF_ALL_ZERO";
		if (chart_options.indexOf(tst_opt) >= 0) {
			ch_options.show_even_if_all_zero = true;
		}
		tst_opt = "SUM_TO_INTERVAL";
		if (chart_options.indexOf(tst_opt) >= 0) {
			ch_options.sum_to_interval = true;
		}
	}
	return ch_options;
}

function can_shape(chrt_idx, use_div, chart_data, tm_beg, hvr_clr, px_high_in, zoom_x0, zoom_x1, zoom_y0, zoom_y1)
{
	if (typeof chart_data === 'undefined') {
		return;
	}
	if ( typeof can_shape === 'undefined' ) {
		let can_shape = {};
	if ( typeof can_shape.sv_pt_prv === 'undefined' ) {
		can_shape.sv_pt_prv = null;
		can_shape.sv_pt     = null;
		can_shape.hvr_arr = [];
	}
	}
	if (typeof can_shape.invocation_num === 'undefined') {
		can_shape.invocation_num = 0;
	}
	can_shape.invocation_num++;
	let px_high = px_high_in;
	if (typeof px_high === 'undefined' || px_high < 100) {
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
	if (chrt_div === null) {
		addElement ('div', use_div, 'chart_anchor', 'before');
		chrt_div = document.getElementById(use_div);
	}
	let minx, maxx, miny, maxy, sldr_cur;
	let draw_mini_box = {};
	let draw_mini_cursor_prev = null;
	let mycanvas2_ctx = null;

	function reset_minx_maxx(zm_x0, zm_x1, zm_y0, zm_y1) {
		let tm_n0 = performance.now();
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
		let tymin = chart_data.y_range.min;

		if (zm_y0 > tymin) {
			miny = zm_y0;
		} else {
			miny = tymin;
		}
		if (typeof chart_data.marker_ymin !== 'undefined') {
			let mrkr_ymin = parseFloat(chart_data.marker_ymin);
			miny = mrkr_ymin;
			//console.log(sprintf("marker_ymin= %f, str= %s zm_y0= %f", mrkr_ymin, chart_data.marker_ymin, zm_y0));
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
		let tm_n01 = performance.now();
		//console.log(sprintf("zm.x0= %f, zm.x1= %f", minx, maxx));
		if (mycanvas2_ctx !== null) {
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
		if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
			let tm_n1 = performance.now();
			console.log(sprintf("__mem: reset mnmx tm= %.2f tm0-1= %.2f", tm_n1-tm_n0, tm_n1-tm_n01));
		}
	}


	//console.log("sldr_cur= "+sldr_cur);
	let myhvr_clr = document.getElementById(hvr_clr);
	let mycanvas_nm_title = "title_"+hvr_clr;
	//myhvr_clr = document.getElementById(hvr_clr);
	if (myhvr_clr === null) {
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
	//console.log(sprintf("ch[%d]= %s", chrt_idx, mycanvas_nm_title));
	let mycanvas_title = document.getElementById(mycanvas_nm_title);
	let ch_title = chart_data.title;
	let ch_options = get_chart_options(chart_data.chart_options);
	let file_tag = chart_data.file_tag;
	if (file_tag.length > 0) {
		ch_title = '<b>'+file_tag+'</b>&nbsp'+ch_title;
	}
	mycanvas_title.innerHTML = ch_title;
	let mycanvas = document.getElementById('canvas_'+hvr_clr);
	if (mycanvas !== null && mycanvas.height != (px_high-4)) {
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
	let follow_arr = [];
	let follow_proc = null
	let event_select = {};
	let event_id_begin = 10000;
	let tot_line = {};
	tot_line.evt_str = [];
	tot_line.lkup  = [];
	tot_line.lkup_ckr= [];
	tot_line.xarray  = [];
	tot_line.yarray  = [];
	tot_line.xarray2 = [];
	tot_line.yarray2 = [];
	tot_line.totals  = [];
	tot_line.evt_str_base_val_arr = [];
	tot_line.evt_str_base_val_hsh = {};
	tot_line.divisions = g_tot_line_divisions.max;
	tot_line.smpl_2_sci = [];
	if (chart_data.tot_line != "") {
		//if (chart_data.chart_tag == "CYCLES_PER_UOP_port_0_CHART")
		if (typeof chart_data.tot_line_opts_xform !== "undefined" &&
			( chart_data.tot_line_opts_xform == "map_cpu_2_core" ||
				chart_data.tot_line_opts_xform == "map_cpu_2_socket")) {
			let u_hsh = {};
			let u_arr = [];
			for (let i=0; i < chart_data.map_cpu_2_core.length; i++) {
				let val;
				if ( chart_data.tot_line_opts_xform == "map_cpu_2_core") {
					val = chart_data.map_cpu_2_core[i].core;
				}
				if (chart_data.tot_line_opts_xform == "map_cpu_2_socket") {
					val = chart_data.map_cpu_2_core[i].socket;
				}
				if (typeof u_hsh[val] === 'undefined') {
					u_hsh[val] = i;
					u_arr.push(val);
				}
			}
			u_arr.sort(function(a, b){return a - b});
			for (let i=0; i < u_arr.length; i++) {
				let str = sprintf("%d", u_arr[i]);
				tot_line.evt_str_base_val_arr.push(u_arr[i]);
				tot_line.evt_str_base_val_hsh[u_arr[i]] = i;
				if (typeof chart_data.tot_line_opts_yvar_fmt !== 'undefined') {
					str = sprintf(chart_data.tot_line_opts_yvar_fmt, u_arr[i]);
				}
				tot_line.evt_str.push(str);
				tot_line.lkup.push([]);
				tot_line.lkup_ckr.push([]);
				tot_line.yarray.push([]);
				tot_line.yarray2.push([]);
				tot_line.xarray2.push([]);
				tot_line.totals.push({});
			}
			/*
			if (chart_data.tot_line_opts_xform == "map_cpu_2_socket") {
				console.log("map_c2s: es_bva: ", tot_line.evt_str_base_val_arr);
				console.log("map_c2s: es: ", tot_line.evt_str);
			}
			*/
		} else if (typeof chart_data.tot_line_opts_xform !== "undefined" &&
			chart_data.tot_line_opts_xform == "select_vars") {
			let u_hsh = {};
			let u_arr = [];
			let num_events = chart_data.subcat_rng.length;
			for (let j=0; j < num_events; j++) {
				let nm     = chart_data.subcat_rng[j].cat_text;
				if (chart_data.subcat_rng[j].total == 0) {
					nm += ", no data";
				}
				if (chart_data.title == "PageFaults and HardFaults") {
					console.log(sprintf("__xform sel_var[%d] %s, ttl= %s", j, nm, chart_data.title));
					console.log("__xform: ", chart_data.subcat_rng[j]);
				}
				u_hsh[j] = nm;
				u_arr.push(j);
			}
			for (let i=0; i < u_arr.length; i++) {
				let str = u_hsh[u_arr[i]];
				tot_line.evt_str_base_val_arr.push(u_arr[i]);
				tot_line.evt_str_base_val_hsh[u_arr[i]] = i;
				if (typeof chart_data.tot_line_opts_yvar_fmt !== 'undefined') {
					str = sprintf(chart_data.tot_line_opts_yvar_fmt, str);
				}
				tot_line.evt_str.push(str);
				tot_line.lkup.push([]);
				tot_line.lkup_ckr.push([]);
				tot_line.yarray.push([]);
				tot_line.yarray2.push([]);
				tot_line.xarray2.push([]);
				tot_line.totals.push({});
			}
			console.log("sel_var tot_line.evt_str.len= "+ tot_line.evt_str.length);
		} else {
			tot_line.evt_str.push(chart_data.tot_line);
			tot_line.lkup.push([]);
			tot_line.lkup_ckr.push([]);
			tot_line.yarray.push([]);
			tot_line.yarray2.push([]);
			tot_line.xarray2.push([]);
			tot_line.totals.push({});
		}
	}
	tot_line.subcat_rng_idx = {};
	tot_line.subcat_rng_arr = [];
	tot_line.event_list_idx = [];
	//console.log("tot_line.evt_str= '"+tot_line.evt_str+"'");

	reset_minx_maxx(zoom_x0, zoom_x1, zoom_y0, zoom_y1);

	let zoom_function = function(task)
	{
		if (!task.do_zoom && !gsync_zoom_linked) {
			return;
		}
		let did_grph=0;
		let i = chrt_idx;
		if (gsync_zoom_arr[i].iter > task.zoom_iter_last) {
			task.zoom_iter_last = gsync_zoom_arr[i].iter;
			let x0 = gsync_zoom_arr[i].x0 - chart_data.ts_initial.ts;
			let x1 = gsync_zoom_arr[i].x1 - chart_data.ts_initial.ts;
			if (typeof gsync_zoom_charts_hash[chrt_idx] === 'undefined') {
				gsync_zoom_charts_hash[chrt_idx] = {tms:0, zoom_iter:task.zoom_iter_last, x0:x0, x1:x1}
			}
			gsync_zoom_charts_hash[chrt_idx].tms++;
			gsync_zoom_charts_hash[chrt_idx].x0 = x0;
			gsync_zoom_charts_hash[chrt_idx].x1 = x1;
			gsync_zoom_charts_redrawn++;
			let tm_now  = performance.now();
			zoom_to_new_xrange(x0, x1, false);
		}
	}
	let zoom_function_obj = {func:zoom_function, typ:"chart", task:{zoom_iter_last:-1, do_zoom:false}};
	gjson.chart_data[chrt_idx].zoom_func_obj = zoom_function_obj;

	function my_wheel_start2(canvas) {
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
	if (ch_type == CH_TYPE_LINE || ch_type == CH_TYPE_STACKED) {
		let mx_cat = null, mnx, mxx, mny, mxy, mx_fe_idx=-1;
		let big_val = 1e6;
		mx_fe_idx = -1;
		if (chart_data.flnm_evt.length > 0) {
			mx_fe_idx = chart_data.flnm_evt[chart_data.flnm_evt.length-1].idx + 1;
		}
		// add a new subcat_rng for the __total__ line
		if (tot_line.evt_str.length > 0) {
			let already_added = false;
			num_events = chart_data.subcat_rng.length;
			/*
			if (chart_data.tot_line_opts_xform == 'select_vars') {
				let icat = chart_data.myshapes[i].ival[IVAL_CAT];
				if (tot_line.evt_str_base_val_arr[sci] != icat) {
					continue;
				}
			}
			*/
			for (let myi=0; myi < tot_line.evt_str.length; myi++) {
			already_added = false;
			let try_total = 0.0;
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
					//try_total += chart_data.subcat_rng[j].total;
					//tot_line.subcat_rng_idx = j;
					//tot_line.subcat_rng_idx[j] = true;
				}
			}
			if (!already_added) {
				let bef_len = chart_data.subcat_rng.length;
				let arr_len = tot_line.subcat_rng_arr.length;
				let use_fe_idx = mx_fe_idx + arr_len;
				let use_mx_cat = mx_cat + arr_len+1;
				let use_total = big_val;
				let use_dura = 0.0;
				let use_base_idx = tot_line.evt_str_base_val_arr[myi];
				if (ch_options.overlapping_ranges_within_area) {
					use_total += chart_data.subcat_rng[use_base_idx].total;
					use_dura +=	chart_data.subcat_rng[use_base_idx].tot_dura;
					if (false) {
					console.log(sprintf("tl.evt[%d]= %s, use_tot= %f, base_totl[%d].evt= %s, tot= %f tot_dura= %s",
								myi, tot_line.evt_str[myi], (use_total-big_val), use_base_idx,
								chart_data.subcat_rng[use_base_idx].cat_text, chart_data.subcat_rng[use_base_idx].total,
								chart_data.subcat_rng[use_base_idx].tot_dura));
					}
				}
				chart_data.subcat_rng.push({x0:mnx, x1:mxx, y0:mny, y1:mxy, fe_idx:use_fe_idx, event:tot_line.evt_str[myi],
					tot_dura:use_dura, total:use_total, is_tot_line:true, is_tot_line_num:myi,
					cat:use_mx_cat, subcat:0, cat_text:tot_line.evt_str[myi]});
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
		if (ch_options.tot_line_legend_weight_by_x_by_y) {
			tot_line_get_values();
			for (let j=0; j < num_events; j++) {
				if (chart_data.subcat_rng[j].is_tot_line) {
					let sci = chart_data.subcat_rng[j].is_tot_line_num;
					let tot_x_by_y = tot_line.totals[sci].tot_x_by_y;
					chart_data.subcat_rng[j].tot_x_by_y = tot_x_by_y;
				}
			}
		}

		for (let j=0; j < num_events; j++) {
			let fe_idx = chart_data.subcat_rng[j].fe_idx;
			event_list.push({event:chart_data.subcat_rng[j].cat_text, idx:chart_data.subcat_rng[j].cat,
				tot_dura:chart_data.subcat_rng[j].tot_dura,
				total:chart_data.subcat_rng[j].total, fe_idx:fe_idx,
				is_tot_line:chart_data.subcat_rng[j].is_tot_line,
				is_tot_line_num:chart_data.subcat_rng[j].is_tot_line_num,
				tot_x_by_y:chart_data.subcat_rng[j].tot_x_by_y,
			});
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
		function sortCat(a, b) {
			return b.total - a.total;
		}
		function sort_x_by_y(a, b) {
			let atd = 0.0;
			let btd = 0.0;
			if (a.is_tot_line === true) {
				atd = a.tot_x_by_y + big_val;
			} else {
				atd = a.tot_dura;
			}
			if (b.is_tot_line === true) {
				btd = b.tot_x_by_y + big_val;
			} else {
				btd = b.tot_dura;
			}
			return btd - atd;
		}
		function sortDura(a, b) {
			let atd = a.tot_dura;
			let btd = b.tot_dura;
			if (a.is_tot_line === true) {
				atd += big_val;
			}
			if (b.is_tot_line === true) {
				btd += big_val;
			}
			return btd - atd;
		}
		if (ch_options.tot_line_legend_weight_by_x_by_y) {
			console.log("sort legend by x_by_y for chart= "+chart_data.chart_tag);
			event_list.sort(sort_x_by_y);
		} else if (ch_options.tot_line_legend_weight_by_dura) {
			console.log("sort legend by dura for chart= "+chart_data.chart_tag);
			event_list.sort(sortDura);
		} else {
			event_list.sort(sortCat);
		}
		if (tot_line.evt_str.length > 0) {
			tot_line.event_list_idx.length = tot_line.subcat_rng_arr.length;
			for (let j=0; j < num_events; j++) {
				if (typeof event_list[j].is_tot_line !== 'undefined' && event_list[j].is_tot_line) {
					if (event_list[j].total == big_val) {
						// can't set it all the way to zero or else it will get dropped later
						event_list[j].total = 1.0e-6; // can't set it all the way to zero or else it will get dropped later
					}
					if (event_list[j].total > big_val) {
						event_list[j].total -= big_val; // can't set it all the way to zero or else it will get dropped later
					}
					for (let k=0; k < tot_line.subcat_rng_arr.length; k++) {
						let sc_idx = tot_line.subcat_rng_arr[k];
						if (chart_data.subcat_rng[sc_idx].cat_text == event_list[j].event) {
							tot_line.event_list_idx[k] = j;
						}
					}
					/*
					*/
					//tot_line.event_list_idx.push(j);
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
	let use_color_list_proc = [];
	let proc_cumu = 0.0;
	let event_cumu = 0.0;
	let proc_rank = {};
	let legend_str = "";
	//let c10 = d3.schemeCategory10;
	// cmd below is what the above cmd accomplishes but I don't have to include d3 anymore
	let c10 = g_d3_clrs_c20;
	let c40 = gcolor_lst;
	let use_color_list = gcolor_lst;
	let legend_text_len = 0;
	let non_zero_legends = 0;
	let has_cpi  = false;
	let has_gips = false;
	let cpi_str = {};
	if (ch_type == CH_TYPE_LINE || ch_type == CH_TYPE_STACKED) {
		if (typeof event_list === 'undefined') {
			console.log("screw up here. event_list not defined. event_list=");
			console.log(event_list);
		}
	  if (event_list.length < c10.length) {
		use_color_list = c10;
		number_of_colors_events = c10.length;
	  } else {
		//use_color_list = gcolor_lst;
		//number_of_colors_events = gcolor_lst.length;
		use_color_list = [];
		for (let i=0; i < event_list.length; i++) {
			use_color_list.push(color_choose_hex_sinebow(i));
		}
		number_of_colors_events = event_list.length;
		//console.log("number of colors for events= "+number_of_colors_events);
	  }
		if (false && ch_options.overlapping_ranges_within_area) {
			console.log(sprintf("legend: e[0]= %s", event_list[0].event));
		}

	  for (let j=0; j < event_list.length; j++) {
		let i = event_list[j].idx; // this is fe_idx
		if (typeof event_select[i] === 'undefined') {
			event_select[i] = [j, 'show'];
		}
		let this_tot = event_list[j].total;
		let tot_dura = event_list[j].tot_dura;
		event_cumu += this_tot;
		let this_pct = 100.0* (this_tot/event_total);
		let pct_cumu = 100.0* (event_cumu/event_total);
		if (this_tot == 0 && !ch_options.show_even_if_all_zero) {
			//drop if all zero here
			continue;
		}
		let fe_idx = event_list[j].idx;
		if (typeof event_list[j].fe_idx !== 'undefined') {
			// so we are doing a non-event list (like a chart by process)
			fe_idx = event_list[j].fe_idx;
		}
		let nm = event_list[j].event;
		let flnm = event_list[j].filename_text;
		if (typeof flnm === 'undefined') {
			//console.log("fe_idx= "+fe_idx+", flnm_evt.sz= "+ chart_data.flnm_evt.length+", ev= "+nm);
			if (typeof fe_idx !== 'undefined' && fe_idx > -1) {
				let fentr = chart_data.flnm_evt[fe_idx];
				let fbin = '';
				let ftxt = '';
				let fevt = '';
			   	if (typeof fentr !== 'undefined') {
					fbin = fentr.filename_bin;
					ftxt = fentr.filename_text;
					fevt = fentr.event;
				} else {
					//console.log("__null fbin for evt= "+nm);
				}
				flnm = "bin:"+fbin + ", txt:"+ftxt + ", event:"+fevt;
			}
		}
		let title = "event["+j+"]= "+nm+" samples: "+this_tot+", tot_dura= "+tot_dura+", %tot_samples= "+this_pct.toFixed(4)+"%, cumu_pct: "+pct_cumu.toFixed(4)+"%, file="+flnm;
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
		if (false && nm == "av.nanosleep") {
			console.log(sprintf("leg_clr: evt= %s, color= %s, ej= %d", nm, clr, j));
		}
		let leg_num = event_list[j].legend_num;
		let evt_str = "";
		if (ch_type == CH_TYPE_LINE) {
			//evt_str = chart_data.y_by_var+" ";
		}
		legend_text_len += evt_str.length + nm.length;

		if (ch_options.overlapping_ranges_within_area) {
			//console.log(sprintf("bef add lgnd for %s tot_ln= %s", nm, event_list[j].is_tot_line));
			if (typeof event_list[j].is_tot_line === 'undefined' || !event_list[j].is_tot_line) {
				//console.log(sprintf("skip leg for %s", nm));
				continue;
			}
		}


		//if (typeof event_list[j].is_tot_line !== 'undefined') {
		//	event_list[j].event_list_idx = j;
		//}
		//if (ch_options.overlapping_ranges_within_area && typeof event_list[j].is_tot_line === 'undefined') {
		//	;
		//} else 
		{
		legend_str += '<span  title="'+title+'" class="'+disp_str+'" style="margin-right:5px; white-space: nowrap; display: inline-block;"><span id="'+hvr_clr+'_legendp_'+leg_num+'" class="legend_square" style="background-color: '+clr+'; display: inline-block"></span><span id="'+hvr_clr+'_legendt_'+leg_num+'">'+evt_str+nm+'</span></span>';
		}
	  }
	} else {
		let cpi_tm = "";
		let cpi_tm2 = "";
		let cpi_cycles = "";
		let cpi_cycles2 = "";
		for (let j=0; j < event_list.length; j++) {
			let nm = event_list[j].event;
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
		if ( typeof event_select[i] === 'undefined') {
			event_select[i] = [j, 'show'];
		}
		let this_tot = event_list[j].total;
		let tot_dura = event_list[j].tot_dura;
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
		if (typeof flnm === 'undefined') {
			if (typeof fe_idx !== 'undefined') {
				flnm = "bin:"+chart_data.flnm_evt[fe_idx].filename_bin + ", txt:"+chart_data.flnm_evt[fe_idx].filename_text;
			}
		}
		let title = "event["+j+"]= "+nm+" samples: "+this_tot+", tot_dura= "+tot_dura+", %tot_samples= "+this_pct.toFixed(4)+"%, cumu_pct: "+pct_cumu.toFixed(4)+"%, file="+flnm;
		let disp_str;
		if (non_zero_legends < 20) {
			disp_str = hvr_clr+"_legend_top_20";
		} else {
			disp_str = hvr_clr+"_legend_top_20_plus";
		}
		use_color_list = [];
		for (let i=0; i < event_list.length; i++) {
			use_color_list.push(color_choose_hex_sinebow(i));
		}
		number_of_colors_events = event_list.length;
		if (j < number_of_colors_events) {
			event_list[j].color = use_color_list[j];
		}
		let clr = event_list[j].color;
		event_list[j].legend_num = event_id_begin+j;
		let leg_num = event_list[j].legend_num;
		legend_text_len += 5 + nm.length;

		legend_str += '<span  title="'+title+'" class="'+disp_str+'" style="margin-right:5px; white-space: nowrap; display: inline-block;"><span id="'+hvr_clr+'_legendp_'+leg_num+'" class="legend_square" style="background-color: '+clr+'; display: inline-block"></span><span id="'+hvr_clr+'_legendt_'+leg_num+'">evt: '+nm+'</span></span>';
	  }
	  if (typeof cpi_str.cycles !== 'undefined' &&
			  typeof cpi_str.instructions !== 'undefined' &&
			  typeof cpi_str.time !== 'undefined') {
		  has_cpi = true;
	  }
	  if ( typeof cpi_str.instructions !== 'undefined' &&
			  typeof cpi_str.time !== 'undefined') {
		  has_gips = true;
	  }
		for (let i=0; i < proc_arr.length; i++) {
			use_color_list_proc.push(color_choose_hex_sinebow(i));
		}
		number_of_colors_proc = proc_arr.length;
	  for (let j=0; j < proc_arr.length; j++) {
		let i = proc_arr[j][1];
		proc_rank[i] = j;
		if ( typeof proc_select[i] === 'undefined') {
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
			clr = use_color_list_proc[j];
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
		if (ele !== null) {
		ele.addEventListener('click', legend_click, false);
		ele.addEventListener('dblclick', legend_dblclick, false);
		ele.addEventListener('mouseenter', legend_mouse_enter, false);
		ele.addEventListener('mouseout', legend_mouse_out, false);
		ele.rank = rank;
		ele.proc_idx = proc_idx;
		ele.typ_legend = typ_legend;
		}
	}
	if (ch_type != CH_TYPE_LINE && ch_type != CH_TYPE_STACKED) {
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
				if (ch_options.overlapping_ranges_within_area) {
					if (typeof event_list[j].is_tot_line === 'undefined' || !event_list[j].is_tot_line) {
						//console.log(sprintf("skip toggle evt[%d]= %s", j, event_list[j].event));
						continue;
					}
					//console.log(sprintf("do toggle evt[%d]= %s", j, event_list[j].event));
				}
				if (i != proc_idx) {
					event_select[i] = [j, tgl_state];
					let leg_num = event_list[j].legend_num;
					let mele_nm = hvr_clr+'_legendt_'+leg_num;
					let mele = document.getElementById(mele_nm);
					if (mele !== null) {
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
			console.log("legend hover id= "+ele_nm+", rank= "+i+", proc_idx= "+proc_idx+", typ= "+typ);
			console.log(sprintf("legend hover event_select[%d] = [%d, %s]", proc_idx, i, 'highlight'));
			chart_redraw("lgnd_hvr_hghlght_evnt");
			event_select[proc_idx] = [i, prev_state];
		}

		if (last_legend_timeout !== null) {
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
		//console.log("__legend_mouse_enter ele= "+ele_nm);
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
		if (last_legend_timeout !== null) {
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
	if (ch_type == CH_TYPE_LINE || ch_type == CH_TYPE_STACKED) {
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
	let lkup_pts_per_pxl = [];
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
		if (g_fl_obj_rt[file_tag_idx][cs_idx] === null && g_fl_obj[file_tag_idx][cs_idx] === null) {
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
			if ((i+1) < csz && g_fl_obj[file_tag_idx][cs_idx].sib_arr[idx].kids === null) {
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
			loop_kk:
			for (let kk=0; kk < lhs_menu_ch_list.length; kk++) {
				for (let ii=0; ii < lhs_menu_ch_list[kk].fl_arr.length; ii++) {
					if (input_cs_evt != "" && lhs_menu_ch_list[kk].fl_arr[ii].event == input_cs_evt) {
						//use_fl_hsh = lhs_menu_ch_list[kk].fl_hsh;
						//use_fl_arr = lhs_menu_ch_list[kk].fl_arr;
						use_fl_hsh = g_fl_hsh[file_tag_idx];
						use_fl_arr = g_fl_arr[file_tag_idx];
						break loop_kk;
					}
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
		if (myck !== null) {
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
		if (mycanvas3 !== null) {
			mycanvas3.parentNode.removeChild(mycanvas3);
			mycanvas3 = document.getElementById(mycanvas3_nm);
		}
		if (mycanvas3 === null) {
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
				//console.log("sum0 lvl"+lvl+" i= "+i+", sum0= "+(1.0e-9*sum0)+", nm= "+fl_obj.sib_arr[i].key+",kids="+(fl_obj.sib_arr[i].kids === null));
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
				if (fl_obj.sib_arr[i].kids !== null) {
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
							if (f_o.dad === null) {break;}
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
		if (g_fl_obj[file_tag_idx][cs_idx] === null) {
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

		function fl_hover(x, y)
		{
			let uchi = mycanvas3.height-0.0;
			let ucwd = mycanvas3.width;
			let xn   = ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * x / ucwd;
			let xnm1 = ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * (x-2.0) / ucwd;
			let xnp1 = ctx3_range.x_min + (ctx3_range.x_max - ctx3_range.x_min) * (x+2.0) / ucwd;
			let yn   = ctx3_range.y_min + (ctx3_range.y_max - ctx3_range.y_min) * (uchi - y)/uchi;
			let ostr2 = fl_lkup_func(xn, yn, x, y, xnm1, xnp1);
			return ostr2;
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
			if (typeof evt_idx_hash[evt] === 'undefined') {
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
						if (typeof cpi_hist_hsh[evt_idx][cpi_str] === 'undefined') {
							cpi_hist_hsh[evt_idx][cpi_str] = cpi_hist_vec[evt_idx].length;
							let ncpi = Math.round(cpi*100)/100;
							if (ncpi == 0.0) {
								console.log("_fl cpi_zero= "+cpi_hist_vec[evt_idx].length);
							}
							cpi_hist_vec[evt_idx].push({cpi:ncpi, sum: 0, smpls:0, smpls1:0, smpls2:0, tm:0, cycl:0, inst:0});
						}
						let h_i = cpi_hist_hsh[evt_idx][cpi_str];
						if (typeof fl_lkup[i][j].fl_obj.sib_arr[k].cpi_idx === 'undefined') {
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
						//if ( fl_lkup[i][j].fl_obj.sib_arr[k].kids === null) {
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
						if (ele !== null) {
							ele.style.display = "none";
						}
					}
					break;
				}
			}
			let fl_title_str = fl_menu_ch_title_str;
			mytitle3.innerHTML = file_tag+fl_title_str+unzoom_str+zoom_str;
			if (g_fl_arr[file_tag_idx][cs_idx].event == "sched:sched_switch" ||
				g_fl_arr[file_tag_idx][cs_idx].event == "CSwitch") {
				ck_if_need_to_save_image(chart_data.chart_tag, mycanvas3, true, fl_hover, mycanvas3_nm_title);
			}
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
							if (f_o.dad === null) {break;}
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
			if ( typeof can_shape.hvr_prv_x === 'undefined' ) {
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
				fl_hover(x, y);
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
				if (typeof ostr2 !== 'undefined' && typeof ostr2.x0 !== 'undefined') {
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
		if (this_ele === null) {
			if (typeof px_high_in === 'undefined' || px_high_in < 100) {
				return gpixels_high_default;
			}
			return px_high;
		} else {
			return this_ele.height;
		}
	}

	if (chart_data.chart_tag == "PCT_BUSY_BY_SYSTEM") {
		console.log("++cpu_busy= "+g_tot_line_divisions.max);
	}

	if (chart_data.chart_tag == "PCT_BUSY_BY_SYSTEM") {
		if (typeof chart_data.follow_proc !== 'undefined' && chart_data.follow_proc.length > file_tag_idx) {
			follow_proc = chart_data.follow_proc[file_tag_idx];
		}
		if (typeof chart_data.follow_proc !== 'undefined' && typeof chart_data.map_cpu_2_core !== 'undefined') {
			console.log(chart_data.follow_proc);
			console.log(sprintf("CPUs= %d, follow= %s", chart_data.map_cpu_2_core.length, follow_proc));
			while(follow_arr.length < chart_data.map_cpu_2_core.length) {
				follow_arr.push({idle:0.0, follow:0.0, other:0.0, follow_proc:follow_proc});
			}
		}
	}
	if (false && ch_options.overlapping_ranges_within_area == true) {
		for (let i=0; i < chart_data.myshapes.length; i++) {
			let x0 = chart_data.myshapes[i].pts[PTS_X0];
			let x1 = chart_data.myshapes[i].pts[PTS_X1];
			chart_data.myshapes[i].pts_sv[PTS_Y0] = chart_data.myshapes[i].pts[PTS_Y0];
			chart_data.myshapes[i].pts_sv[PTS_Y1] = chart_data.myshapes[i].pts[PTS_Y1];
			let cpu = chart_data.myshapes[i].ival[IVAL_CPU];
			let fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
			let do_event = false;
			if (fe_idx == -1 || (typeof event_select[fe_idx] !== 'undefined' &&
				(event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show'))) {
				do_event = true;
			}
			if (!do_event) {
				continue;
			}
		}
	}

	function tot_line_get_values()
	{
		let ret_data = {};
		let maxy_new = maxy;
		ret_data = {maxy_new: maxy_new, typ: "def"};
		if (tot_line.evt_str.length == 0) {
			return ret_data;
		}
		if (typeof follow_arr !== 'undefined') {
			for (let i=0; i < follow_arr.length; i++) {
				follow_arr[i].idle = 0.0;
				follow_arr[i].follow = 0.0;
				follow_arr[i].other  = 0.0;
			}
		}
		maxy_new = null;
		let fld_1st_time = {};
		let HT_enabled = false;
		let desc = "";
		let map_num_den = 0;
		let scope_arr = [];
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
			(chart_data.tot_line_opts_xform == 'map_cpu_2_core' ||
			chart_data.tot_line_opts_xform == 'map_cpu_2_socket')) {
			// I probably should have a way to indicate if really want to divide by 2 for HT
			// The 'div by 2' is for the case where we are computing cycles/uop with HT enabled
			// then summing core thread 0 and 1.
			for(let cpu=0; cpu < chart_data.map_cpu_2_core.length; cpu++) {
				if (chart_data.map_cpu_2_core[cpu].core != cpu) {
					HT_enabled = true;
					break;
				}
			}
		}
		let HT_factor_num = 1.0, HT_factor_den = 1.0;
		let scope = {"num":null, "den":null};
		if (typeof chart_data.tot_line_opts_scope !== 'undefined') {
			scope_arr = chart_data.tot_line_opts_scope;
		}
		if (scope_arr.length > 0) {
			if (scope_arr.length >= 1) {
				scope.num = scope_arr[0];
			}
			if (scope_arr.length >= 2) {
				scope.den = scope_arr[1];
			}
			if (HT_enabled) {
				if (scope_arr.length >= 1 && scope_arr[0] == 'per_core') {
					HT_factor_num = 0.5;
				}
				if (scope_arr.length >= 2 && scope_arr[1] == 'per_core') {
					HT_factor_den = 0.5;
				}
				//console.log(sprintf("n= %.1f, d= %.1f mnd= %d ttl= %s", HT_factor_num, HT_factor_den, map_num_den, chart_data.title));
			} else {
				HT_factor_num = 1.0;
				HT_factor_den = 1.0;
			}
		}
		let num=0.0, den=0.0;
		if (ch_type == CH_TYPE_LINE || ch_type == CH_TYPE_STACKED) {
			if (tot_line.divisions != (g_tot_line_divisions.max)) {
				tot_line.divisions = g_tot_line_divisions.max;
			}
			let do_map_cpu_2_core = false, do_map_cpu_2_socket = false, do_sel_vars = false;
			if (tot_line.evt_str_base_val_arr.length > 0) {
				if (typeof chart_data.tot_line_opts_xform !== 'undefined') {
					if (chart_data.tot_line_opts_xform == 'map_cpu_2_core' ||
						chart_data.tot_line_opts_xform == 'map_cpu_2_socket') {
						if (chart_data.tot_line_opts_xform == 'map_cpu_2_core') {
							do_map_cpu_2_core = true;
						}
						else if (chart_data.tot_line_opts_xform == 'map_cpu_2_socket') {
							do_map_cpu_2_socket = true;
						}
					} else if (chart_data.tot_line_opts_xform == 'select_vars') {
						do_sel_vars = true;
					}
				}
			}
			let tm_top_00 = performance.now();
			tot_line.xarray.length = tot_line.divisions+1;
			tot_line.xarray.fill(0.0);
			let tot_dura = [];
			let tot_pts = 0;
			let tm_loop = 0.0;
			tot_dura.length = tot_line.evt_str.length;
			for (let sci= 0; sci < tot_line.evt_str.length; sci++) {
				if (tot_line.lkup[sci].length != tot_line.divisions+1) {
					tot_line.lkup[sci].length = tot_line.divisions+1;
					//tot_line.lkup_ckr[sci].length = tot_line.divisions+1;
					tot_line.xarray2[sci].length = tot_line.divisions+1;
					tot_line.yarray2[sci].length = tot_line.divisions+1;
					tot_line.yarray[sci].length = tot_line.divisions+1;
				}
				tot_line.yarray[sci].fill(0.0);
				tot_line.yarray2[sci].fill(0.0);
				tot_line.xarray2[sci].fill(0.0);
				tot_line.totals[sci] = {tot:0.0, tot_x_by_y:0.0};
				for (let j=0; j < tot_line.yarray[sci].length; j++) {
					tot_line.lkup[sci][j] = [];
					//tot_line.lkup_ckr[sci][j] = {};
				}
				tot_dura[sci] = {dura:0.0, smpl:0};
			}
			if (tot_line.smpl_2_sci.length == 0) {
				tot_line.smpl_2_sci.length = chart_data.myshapes.length;
				for (let i=0; i < chart_data.myshapes.length; i++) {
					let usci = -1;
					let fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
					let cpu = chart_data.myshapes[i].ival[IVAL_CPU];
					if (do_sel_vars) {
						usci = tot_line.evt_str_base_val_hsh[chart_data.myshapes[i].ival[IVAL_CAT]];
					} else if (do_map_cpu_2_core) {
						usci = tot_line.evt_str_base_val_hsh[chart_data.map_cpu_2_core[cpu].core];
					} else if (do_map_cpu_2_socket) {
						usci = tot_line.evt_str_base_val_hsh[chart_data.map_cpu_2_core[cpu].socket];
					} else {
						usci = 0;
					}
					tot_line.smpl_2_sci[i] = usci;
				}
			}
			//for (let sci= 0; sci < tot_line.evt_str.length; sci++)
			{
				let sci;
				for (let i=0; i < chart_data.myshapes.length; i++) {
					sci = tot_line.smpl_2_sci[i];
					let fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
					let do_event = false;
					let frst_tm = false;
					if (typeof event_list === 'undefined' || event_list.length == 0) {
						frst_tm = true;
					}
					if (!frst_tm) {
					if (fe_idx == -1 || (typeof event_select[fe_idx] !== 'undefined' &&
						(event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show'))) {
						do_event = true;
					}
					if (!do_event) {
						continue;
					}
					}
					let cpu = chart_data.myshapes[i].ival[IVAL_CPU];
					if (do_sel_vars) {
						if (chart_data.myshapes[i].ival[IVAL_CAT] != tot_line.evt_str_base_val_arr[sci]) {
							continue;
						}
					} else if (do_map_cpu_2_core) {
						if (chart_data.map_cpu_2_core[cpu].core != tot_line.evt_str_base_val_arr[sci]) {
							continue;
						}
					} else if (do_map_cpu_2_socket) {
						if (chart_data.map_cpu_2_core[cpu].socket != tot_line.evt_str_base_val_arr[sci]) {
							continue;
						}
					}
					let usci = tot_line.smpl_2_sci[i];
					if (usci != sci) {
						console.log(sprintf("usci= %d, sci= %d, no match", usci, sci));
						fail;
					}
					let x0 = chart_data.myshapes[i].pts[PTS_X0];
					let x1 = chart_data.myshapes[i].pts[PTS_X1];
					if (x1 < minx || x0 > maxx) {
						continue;
					}
					//let tm_top_20 = performance.now();
					let tx0 = x0;
					let tx1 = x1;
					if (x0 < minx) {
						tx0 = minx;
					}
					if (x1 > maxx) {
						tx1 = maxx;
					}
					tot_dura[sci].dura += tx1 - tx0;
					tot_dura[sci].smpl += 1;
						if (sci == 189) {
							tot_pts++;
						}
					//let tm_top_21 = performance.now();
					//tm_loop += tm_top_21 - tm_top_20;
					if (chart_data.chart_tag == "PCT_BUSY_BY_SYSTEM") {
						let icpu = chart_data.myshapes[i].ival[IVAL_CPU];
						let cpt = chart_data.myshapes[i].ival[IVAL_CPT];
						let comm=null, pid=-1, tid=-1;
						if (cpt > -1) {
							comm = chart_data.proc_arr[cpt].comm;
							pid  = chart_data.proc_arr[cpt].pid;
							tid  = chart_data.proc_arr[cpt].tid;
						}
						let tstr = null;
						let txtidx = chart_data.myshapes[i].txtidx;
						if (typeof txtidx !== 'undefined') {
							tstr = gjson_str_pool.str_pool[0].strs[txtidx];
						}
						if (typeof follow_arr !== 'undefined') {
							if (follow_arr.length > 0) {
								if (pid == 0 && tid == 0) {
									follow_arr[icpu].idle += tx1 - tx0;
								} else if (tstr !== null && follow_proc !== null) {
									if (tstr.indexOf(follow_proc) >= 0) {
										follow_arr[icpu].follow += tx1 - tx0;
									} else {
										follow_arr[icpu].other += tx1 - tx0;
									}

								} else {
									follow_arr[icpu].other += tx1 - tx0;
								}
							}
						}
					}
					let y0 = chart_data.myshapes[i].pts[PTS_Y0];
					let y1 = chart_data.myshapes[i].pts[PTS_Y1];
					let yval, num, den;
					if (ch_type == CH_TYPE_LINE) {
						yval = y1;
						//if (tot_line.evt_str[sci]  == "av.nanosleep") {
							//console.log(sprintf("nano: y0: %.3f, y1: %.3f", y0, y1));
						//}
						if (map_num_den > 0) {
							num = chart_data.myshapes[i].num;
							den = chart_data.myshapes[i].den;
						}
					} else {
						yval = (y1 - y0);
					}

					// compute relative relx0/1 where x0 is [0.0, 1.0] over minx->maxx
					let relx0 = tx0 - minx;
					let relx1 = tx1 - minx;
					relx0 /= (maxx - minx);
					relx1 /= (maxx - minx);
					// nrx0/1 is index for bucket
					let nrx0 = tot_line.divisions * relx0;
					let nrx1 = tot_line.divisions * relx1;
					// get starting and ending bucket index
					let xbeg = Math.floor(nrx0);
					let xend = Math.ceil(nrx1);
					let ibeg = Math.trunc(xbeg);
					let iend = Math.trunc(xend);
					if (ch_options.tot_line_bucket_by_end_of_sample && ibeg < (iend-1)) {
						ibeg = iend-1;
					}
					if (ch_options.sum_to_interval) {
						ibeg = iend-1;
					}
					//console.log("i= "+i+", beg= "+ibeg+", end= "+iend);
					for (let j=ibeg; j < iend; j++) {
						let xcur0, xcur1;
						//  calc beg and end x of current j'th interval
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
						tot_line.lkup[sci][j].push(i);
						if (map_num_den > 0) {
							let y_num, x_den;
							if (!ch_options.tot_line_add_values_in_interval) {
								y_num = HT_factor_num * num * (xcur1 - xcur0) * tot_line.divisions;
								x_den = HT_factor_den * den * (xcur1 - xcur0) * tot_line.divisions;
							} else {
								y_num = HT_factor_num * num;
								x_den = HT_factor_den * den;
							}
							let use_sci_x = sci;
							let use_sci_y = sci;
							let use_cpu0_x = false, use_cpu0_y = false;
							if (scope.num !== null) {
								if (scope.num == "use_cpu0") {
									use_cpu0_y = true;
								}
							}
							if (!use_cpu0_y || cpu == 0) {
								tot_line.yarray2[sci][j] += y_num;
							}
							if (scope.den !== null) {
								if (scope.den == "use_cpu0") {
									use_cpu0_x = true;
								}
							}
							if (!use_cpu0_x || cpu == 0) {
								tot_line.xarray2[sci][j] += x_den;
							}
							if (ch_options.sum_to_interval) {
								tot_line.yarray[sci][j] += yval;
							} else if (tot_line.xarray2[sci][j] > 0.0) {
								tot_line.yarray[sci][j] = tot_line.yarray2[sci][j]/tot_line.xarray2[sci][j];
							} else {
								tot_line.yarray[sci][j] = 0.0;
							}
						} else {
							if (ch_options.sum_to_interval || ch_options.tot_line_add_values_in_interval) {
								tot_line.yarray[sci][j] += yval;
							} else {
								// calc the fraction of yval in the current interval
								tot_line.yarray[sci][j] += yval * (xcur1 - xcur0) * tot_line.divisions;
							}
						}
					}
				}
			}
			let tm_top_02 = performance.now();
			if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
				console.log(sprintf("__mem get_tot_val tm0= %.2f, scim= %d, div= %d, tm_loop= %.2f",
					tm_top_02-tm_top_00, tot_line.yarray.length, tot_line.divisions, tm_loop));
			}
			for (let sci= 0; sci < tot_line.evt_str.length; sci++) {
				let tot_avg = 0, tot_avg2= 0;
				let tot_avg2_num = 0;
				let tot_avg2_den = 0;
				let do_event = false;
				let el_idx = tot_line.event_list_idx[sci];
				let fe_idx = -1;
				let frst_tm = false;
				if (typeof event_list === 'undefined' || event_list.length == 0) {
					frst_tm = true;
				}

				if (!frst_tm) {
					if (el_idx < 0 || typeof event_list[el_idx] === 'undefined') {
						//console.log(sprintf("event_list[%s] undef for sci= %s of chart= %s", el_idx, sci, chart_data.chart_tag));
							continue;
					}
					fe_idx = event_list[el_idx].idx;
					if (fe_idx == -1 || (typeof event_select[fe_idx] !== 'undefined' &&
						(event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show'))) {
						do_event = true;
					}
				}
				for (let j=0; j < (tot_line.divisions+1); j++) {
					tot_line.xarray[j] = minx + j * (maxx - minx) / tot_line.divisions;
					tot_avg += tot_line.yarray[sci][j];
					tot_avg2_num += tot_line.yarray2[sci][j];
					tot_avg2_den += tot_line.xarray2[sci][j];
					if (do_event && (maxy_new === null || maxy_new <= tot_line.yarray[sci][j])) {
						maxy_new = tot_line.yarray[sci][j];
						ret_data = {maxy_new: maxy_new, typ: "ck"}
					}
				}
				let tot_x_by_y = tot_avg;
				tot_line.totals[sci] = {tot:tot_avg, tot_x_by_y:tot_x_by_y};
				if (tot_line.divisions > 0) {
					if (!ch_options.tot_line_add_values_in_interval) {
						tot_avg /= tot_line.divisions;
					}
					if (tot_avg2_den > 0.0) {
						tot_avg2 = tot_avg2_num/tot_avg2_den;
					} else {
						tot_avg2 = 0.0;
					}
					/*
					if (chart_data.chart_tag == "SYSCALL_ACTIVE_CHART") {
						console.log(sprintf("tot_avg[%d]= %.3f tot_dura= %.3f smpl= %.0f, area= %s\n",
									sci, tot_avg, tot_dura[sci].dura, tot_dura[sci].smpl, tot_line.evt_str[sci]));
					   //	chart_data.title));
					}
					*/
					if (g_cpu_diagram_flds !== null) {
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
								g_cpu_diagram_flds.cpu_diagram_fields[j].chrt_idx = chrt_idx;
								g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line = tot_line;
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
		if (chart_data.chart_tag == "PCT_BUSY_BY_SYSTEM" && typeof follow_arr !== 'undefined') {
			let x_rng = maxx - minx;
			for (let i=0; i < follow_arr.length; i++) {
				let tot = follow_arr[i].idle + follow_arr[i].follow + follow_arr[i].other;
				if (tot < x_rng) {
					follow_arr[i].idle += x_rng - tot;
				}
			}
			if (g_cpu_diagram_flds !== null) {
				for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
					if (chart_data.chart_tag == g_cpu_diagram_flds.cpu_diagram_fields[j].chart) {
						g_cpu_diagram_flds.cpu_diagram_fields[j].follow_arr = follow_arr;
					}
				}
			}
			console.log("follow_arr: ", follow_arr);
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
		let tm_here_04a = performance.now();
		let tl_maxy_new = tot_line_get_values();
		let maxy_new = tl_maxy_new.maxy_new;
		let tm_here_04ab = performance.now();
		redo_ylkup(ctx);
		let tm_here_04ac = performance.now();
		let build_fl_tm = 0.0;
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
		if (ch_type == CH_TYPE_LINE || ch_type == CH_TYPE_STACKED) {
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
				if (fe_idx == -1 || (typeof event_select[fe_idx] !== 'undefined' &&
					(event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show'))) {
					do_event = true;
				}
				if (!do_event) {
					continue;
				}
				let y0 = chart_data.myshapes[i].pts[PTS_Y0];
				let y1 = chart_data.myshapes[i].pts[PTS_Y1];
				if (tmaxy === null) {
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
			if (typeof chart_data.marker_ymin !== 'undefined') {
				let mrkr_ymin = parseFloat(chart_data.marker_ymin);
				tminy = mrkr_ymin;
			}
			if (tmaxy !== null) {
				if (tminy == tmaxy) {
					if (tminy != 0.0) {
						tmaxy *= 1.05;
						tminy *= 0.95;
					} else {
						tmaxy =  0.1;
						tminy = -0.1;
					}
				}
				uminy = tminy;
				umaxy = tmaxy;
				if (umaxy < maxy_new && tl_maxy_new.typ != "def") {
					umaxy = tl_maxy_new.maxy_new;
				}
				if (ch_options.overlapping_ranges_within_area) {
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
		//abcd
		lkup_pts_per_pxl = [];
		for (let i=0; i < chart_data.subcat_rng.length; i++) {
			line_done.push({});
			lkup.push([]);
			lkup_pts_per_pxl.push([]);
			for (let j=0; j <= px_wide; j++) {
				lkup_pts_per_pxl[i].push(0.0);
			}
			step.push([-1, -1]);
		}
		let filtering = false;
		if (ch_type != CH_TYPE_LINE && ch_type != CH_TYPE_STACKED) {
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
		gjson.chart_data[chrt_idx].last_used_x_min_max = {minx:minx, maxx:maxx};
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
			if (x0 == x1 && ch_type == CH_TYPE_BLOCK) {
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
                if (fe_idx < 0 || typeof fe_idx === 'undefined') {
                    //console.log("fe_idx= "+fe_idx);
                }
				let fe_2 = event_lkup[fe_idx];
                if (fe_2 < 0 || typeof fe_2 === 'undefined') {
                    //console.log("fe_2= "+fe_2);
                }
				let ev = "unknown3";
                if (typeof fe_2 !== 'undefined') {
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
			if (ch_options.overlapping_ranges_within_area) {
				let nmi = chart_data.myshapes[i].ival[IVAL_CAT];
				if (typeof chart_data.subcat_rng[nmi].is_tot_line === 'undefined' ||
					!chart_data.subcat_rng[nmi].is_tot_line) {
					continue;
				}
				/*
				*/
				/* 
				 * this method causes
				let fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
				if (fe_idx == -1 || (typeof event_select[fe_idx] !== 'undefined')) {
					event_select[fe_idx][1] = 'hide';
				}
				*/
			}
			let drew_this_shape = false;
			if (ch_type == CH_TYPE_BLOCK && chart_data.myshapes[i].ival[IVAL_SHAPE] == SHAPE_RECT) {
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
					if (typeof proc_select[idx] !== 'undefined' &&
						(proc_select[idx][1] == 'highlight' || proc_select[idx][1] == 'show')) {
						if (rnk < number_of_colors_proc) {
							ctx.fillStyle = use_color_list_proc[rnk];
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
				if (ch_type == CH_TYPE_LINE) {
					fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
				} else if (ch_type == CH_TYPE_STACKED) {
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
                if (typeof fe_rnk !== 'undefined') {
				    clr = event_list[fe_rnk].color;
                }
				let do_proc = false, do_event = false, do_event_highlight = false;
				if (idx == -1 || (typeof proc_select[idx] !== 'undefined' &&
					(proc_select[idx][1] == 'highlight' || proc_select[idx][1] == 'show'))) {
					do_proc = true;
				}
				if (fe_idx == -1 || (typeof event_select[fe_idx] !== 'undefined' &&
					(event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show'))) {
					do_event = true;
					if (typeof event_select[fe_idx] !== 'undefined' && event_select[fe_idx][1] == 'highlight') {
						do_event_highlight = true;
					}
				}
				if ((do_proc && ch_type == CH_TYPE_BLOCK) || (do_event &&
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
                    if (typeof fe_2 !== 'undefined') {
					    ev = event_list[fe_2].event;
                    }
					let has_cs = false;
					if (typeof chart_data.myshapes[i].cs_strs !== 'undefined'
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
								cs_clr =  use_color_list_proc[rnk];
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
				if (ch_type == CH_TYPE_STACKED) {
					let fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
						//fe_idx = chart_data.myshapes[i].ival[IVAL_FE];
					let do_event_highlight = false;
					if (fe_idx == -1 || (typeof event_select[fe_idx] !== 'undefined' &&
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
				} else if (ch_type == CH_TYPE_LINE || line_done[subcat_idx][beg[0]] != 1) {
					let do_event = false;
					let fe_idx = chart_data.myshapes[i].ival[IVAL_CAT];
					if (ch_type == CH_TYPE_BLOCK) {
						fe_idx = chart_data.myshapes[i].ival[IVAL_FE];
					}
					if (fe_idx == -1 || (typeof event_select[fe_idx] !== 'undefined' &&
						(event_select[fe_idx][1] == 'highlight' || event_select[fe_idx][1] == 'show'))) {
						do_event = true;
						if (fe_idx != -1 && event_select[fe_idx][1] == 'highlight') {
							do_event_highlight = true;
						}
					}
					if ((ch_type != CH_TYPE_LINE && do_proc) || do_event) {
						// the line_done array is 1 if we've already drawn a line at this pixel. Don't need to overwrite it.
						ctx.beginPath();
						ctx.strokeStyle = clr;
						if  (ch_type == CH_TYPE_LINE) {
							ctx.lineWidth = 1;
						}
						let do_step = g_do_step;
						if (ch_type != CH_TYPE_LINE) {
							do_step = false;
						}
						let connect_lines = true;
						if (typeof chart_data.marker_connect !== 'undefined' && !g_do_step_changed) {
							if (chart_data.marker_connect == "y") {
								do_step = true;
							}
							if (chart_data.marker_connect == "n") {
								do_step = false;
								connect_lines = false;
							}
						}
						let draw_marker = false;
						if (typeof chart_data.marker_type !== 'undefined' && typeof chart_data.marker_size !== 'undefined') {
							if (chart_data.marker_type == "square") {
								draw_marker = true;
							}
						}
						if (typeof chart_data.marker_text !== 'undefined' && chart_data.marker_text == "y") {
							let tx_mid = x0     + 0.5*(x1-x0);
							let abs_ts = (tx_mid + chart_data.ts_initial.ts - chart_data.ts_initial.ts0x);
							let str5 = get_phase_indx(abs_ts, false);
							if (str5.indexOf("idle: ") < 0) {
								let cma = str5.indexOf(",");
								if (cma > 0) {
									str5 = str5.substr(0, cma);
								}
								let ck_str = ["Single-Core ", "Multi-Core "];
								for (let ck=0; ck < ck_str.length; ck++) {
									let sngl = str5.indexOf(ck_str[ck]);
									if (sngl >= 0) {
										str5 = str5.substr(ck_str[ck].length);
										break;
									}
								}
								let tx_pxl = beg[0] + 0.5*(end[0]-beg[0]);
								ctx.save();
								ctx.translate(tx_pxl, end[1]);
								ctx.textAlign = "center";
								ctx.rotate(-0.5*Math.PI);
								//console.log(sprintf("abs_ts= %f str5= %s", abs_ts, str5));
								ctx.fillText(str5, 0, 0);
								ctx.restore();
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
										//abcd
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
							if  (ch_type == CH_TYPE_LINE) {
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
				for (let j=beg[0]; j <= end[0]; j++) {
					lkup_pts_per_pxl[subcat_idx][j] += 1;
				}
				if (false) {
				}
			}
		}
		let tm_here_04a1 = performance.now();
		// now draw __total__ line if needed
		if (tot_line.evt_str.length > 0) {
			//for (let sci= 0; sci < tot_line.evt_str.length; sci++)
			for (let sci= 0; sci < tot_line.subcat_rng_arr.length; sci++) {
			//let sc_idx = tot_line.subcat_rng_idx;
			let sc_idx = tot_line.subcat_rng_arr[sci];
			let el_idx = tot_line.event_list_idx[sci];
			let fe_idx = event_list[el_idx].idx;
			//let fe_idx = event_list[tot_line.event_list_idx[sci]].fe_idx;
			//console.log("tot_line: sc_idx= "+sc_idx+", yarr= "+tot_line.yarray.length+", ch_title="+chart_data.title+", fe_idx= "+fe_idx);
			let cat = chart_data.subcat_rng[sc_idx].cat;
			let subcat = chart_data.subcat_rng[sc_idx].subcat;
			let subcat_idx = subcat_cs_2_sidx_hash[cat][subcat];
			//let clr = event_list[el_idx].color;
			let clr = event_list[el_idx].color;
			if (false && chart_data.subcat_rng[sc_idx].cat_text == "av.nanosleep") {
			console.log(sprintf("tot_line[%d]: sci= %d sc_idx= %d, fe_idx= %d, el_idx= %d, text= %s, subcat_idx= %d, clr= %s",
				el_idx, sci, sc_idx, fe_idx, el_idx, tot_line.evt_str[sci], subcat_idx, clr));
			}
			//console.log("tot_line: sc_idx= "+sc_idx+", fe_idx= "+fe_idx+", fe_rnk= "+fe_rnk+", clr= "+clr+", subcat_idx= "+subcat_idx);
			let do_event = false;
			let do_event_highlight = false;
			//console.log("ev_lst_idx= "+tot_line.event_list_idx[sci]+", fe_idx= "+fe_idx);
			if (fe_idx != -1 && typeof event_select[fe_idx] !== 'undefined') {
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
		if ((ch_type == CH_TYPE_BLOCK && chart_data.chart_tag == "PCT_BUSY_BY_CPU") ||
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
			if (1==20 && build_flame_rpt_timeout !== null) {
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
		ck_if_need_to_set_zoom_redrawn_cntr("redraw");
		ck_if_need_to_save_image(chart_data.chart_tag, mycanvas, false, can_hover, mycanvas_nm_title);
		if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
			let tm_n = performance.now();
			console.log(sprintf("__mem: soc totTm= %.1f, rdrw= %.1f, tt_ln= %.1f rest= %.1f get_tot_lns= %.1f, ylkup= %.1f x:mn= %.1f, mx= %.1f, from_where= %s",
				tm_n - tm_here_04a, tm_here_04a1 - tm_here_04ac, tm_here_04b - tm_here_04a1, tm_n - tm_here_04b,
				tm_here_04ab - tm_here_04a, tm_here_04ac - tm_here_04ab, minx, maxx,
				from_where));
		}
	}
	function ck_if_need_to_set_zoom_redrawn_cntr(from)
	{
		if (gsync_zoom_redrawn_charts_map[chrt_idx] == 0) {
			gsync_zoom_redrawn_charts_map[chrt_idx] = 1;
			//console.log(sprintf("ck_if_rdrw[%d] from= %s", chrt_idx, from));
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

	function ck_if_need_to_save_image(ch_tag, use_canvas, doing_flame, hover_func, ele_title_nm)
	{
		if (g_cpu_diagram_flds !== null) {
			let use_j = -1;
			let lkfor_chart = null;
			for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
				if (ch_tag == g_cpu_diagram_flds.cpu_diagram_fields[j].chart_tag &&
					typeof g_cpu_diagram_flds.cpu_diagram_fields[j].save_image_nm !== 'undefined') {
					lkfor_chart = g_cpu_diagram_flds.cpu_diagram_fields[j].chart;
					use_j = j;
					break;
				}
			}
			if (lkfor_chart == null && ch_tag == "PCT_BUSY_BY_CPU") {
				lkfor_chart = "PCT_BUSY_BY_CPU";
			}
			for (let j=0; j < gjson.chart_data.length; j++) {
				if (lkfor_chart == gjson.chart_data[j].chart_tag) {
					if (doing_flame) {
						gjson.chart_data[j].fl_image_shape = {canvas3:use_canvas, wide:use_canvas.width, high:use_canvas.height-0.0, ele_title_nm:ele_title_nm};
						gjson.chart_data[j].fl_image_hover = hover_func;
						gjson.chart_data[j].fl_image_ready = false;
						gjson.chart_data[j].fl_image = new Image();
						gjson.chart_data[j].fl_image.src = use_canvas.toDataURL("image/png");
						gjson.chart_data[j].fl_image.onload = function(){
							gjson.chart_data[j].fl_image_ready = true;
						}
					} else {
						if (typeof gjson.chart_data[j].image_data === 'undefined') {
							gjson.chart_data[j].image_data = {};
						}
						if (!(ch_tag in gjson.chart_data[j].image_data)) {
							gjson.chart_data[j].image_data = {shape:null, hover:null};
						}
						gjson.chart_data[j].image_data.shape = {canvas3:use_canvas, wide:use_canvas.width, high:use_canvas.height-0.0, ele_title_nm:ele_title_nm};
						gjson.chart_data[j].image_data.hover = hover_func;
						gjson.chart_data[j].image_ready = false;
						gjson.chart_data[j].image = new Image();
						//console.log("going to save image_nm for chart["+j+"]= "+chart_data.chart_tag);
						gjson.chart_data[j].image.src = use_canvas.toDataURL("image/png");
						gjson.chart_data[j].image.onload = function(){
							gjson.chart_data[j].image_ready = true;
						}
					}
					if (use_j != -1) {
						g_cpu_diagram_flds.cpu_diagram_fields[use_j].gjson_chart_data_img_idx = j;
					}
					break;
				}
			}
		}
	}

	function draw_mini(zero_to_one, arg2)
	{
		if (isNaN(zero_to_one)) {
			return;
		}
		let tm_n00 = performance.now();
		//console.log("draw_mini["+chrt_idx+"]("+zero_to_one+", arg= "+arg2+")");
		if ( typeof draw_mini.x_prev === 'undefined' ||
			draw_mini.invocation_num != can_shape.invocation_num) {
			draw_mini.x_prev = -1;
			if (typeof draw_mini.invocation_num === 'undefined') {
			draw_mini.image_rdy = 0;
			draw_mini.image = new Image();
			draw_mini.image.src = mycanvas.toDataURL("image/png");
			//draw_mini.image.src = mycanvas.toDataURL();
			if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
				let tm_n01 = performance.now();
				console.log(sprintf("__mem nw_img tm= %.2f", tm_n01-tm_n00));
			}

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
				if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
					let tm_n01 = performance.now();
					console.log(sprintf("__mem on_ld tm= %.2f", tm_n01-tm_n00));
				}
				draw_mini(xx, "b");
				if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
					let tm_n01 = performance.now();
					console.log(sprintf("__mem drw_mini tm= %.2f, invo= %s", tm_n01-tm_n00, can_shape.invocation_num));
				}
			};
			}
			draw_mini.invocation_num = can_shape.invocation_num;
			//console.log("__draw_mini save image");
		}
		//if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
		//	let tm_n01 = performance.now();
		//	console.log(sprintf("__mem drw_mini_a tm= %.2f, invo= %s", tm_n01-tm_n00, can_shape.invocation_num));
		//}

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
		//if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
		//	let tm_n01 = performance.now();
		//	console.log(sprintf("__mem drw_mini_tot_m1 tm= %.2f, invo= %s", tm_n01-tm_n00, can_shape.invocation_num));
		//}
		zoom_to_new_xrange(x0, x1, true);
		if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
			let tm_n01 = performance.now();
			console.log(sprintf("__mem drw_mini_tot tm= %.2f, invo= %s", tm_n01-tm_n00, can_shape.invocation_num));
		}
	}
	if (chart_did_image[chrt_idx] === null || copy_canvas == true) {
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
			if (draw_mini_cursor_prev !== null) {
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
	function linearSearch(ar, elx, ely, xscale, yscale, compare_fn) {
		for (let k=0; k < ar.length; k++) {
			let cmp = compare_fn(elx, ely, xscale, yscale, ar[k]);
			if (cmp == 0) {
				return k; 
			}
		}
		return -1;
	}
	function binarySearch(ar, elx, ely, xscale, yscale, compare_fn) {
	    let m = 0;
	    let n = ar.length - 1;
	    while (m <= n) {
			let k = (n + m) >> 1;
			let cmp = compare_fn(elx, ely, xscale, yscale, ar[k]);
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
	function compare_in_rect(x, y, xscale, yscale, b) {
		if (x  < (b[0]-1.0*yscale)) {
			return -1;
		}
		if (x  > (b[2]+1.0*yscale)) {
			return 1;
		}
		return 0;
		//return a - b;
	}
	function compare_in_box(x, y, xscale, yscale, b) {
		if (x  < (b[0]-2*xscale)) {
			return -1;
		}
		if (x  > (b[2]+2*xscale)) {
			return 1;
		}
		if (y  < (b[1]-2*yscale)) {
			return 1;
		}
		if (y  > (b[3]+2*yscale)) {
			return -1;
		}
		return 0;
		//return a - b;
	}
	function compare_in_stack(x, y, xscale, yscale, b) {
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
		if (typeof chart_data.myshapes[idx].cs_strs === 'undefined') {
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
		if (typeof comm !== 'undefined' && comm !== null && comm != "") {
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
			let tm_ms_up1 = performance.now();
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
					if (typeof chart_data.myshapes[shape_idx] === 'undefined') {
						console.log(sprintf("!!!! shape_idx= %d but len= %d", shape_idx, chart_data.myshapes.length));
					} else {
					let intrvl_x0 = chart_data.myshapes[shape_idx].pts[PTS_X0]+chart_data.ts_initial.ts - chart_data.ts_initial.ts0x;
					let intrvl_x1 = chart_data.myshapes[shape_idx].pts[PTS_X1]+chart_data.ts_initial.ts - chart_data.ts_initial.ts0x;
					str4 = "<br>abs.T= "+intrvl_x0+ " - " + intrvl_x1;
					}
				}
				let rel_nx0 = nx0 + chart_data.ts_initial.tm_beg_offset_due_to_clip;
				let abs_ts = (nx0 + chart_data.ts_initial.ts - chart_data.ts_initial.ts0x);
				let str5 = get_phase_indx(abs_ts, false);
				console.log(sprintf("abs_ts= %f str5= %s", abs_ts, str5));
				if (str5 != "") {
					str5 = "<br>Phase= "+str5;
				}
				set_chart_text(myhvr_clr_txt, myhvr_clr_txt_btn, "x= " + x + ", rel.T= "+rel_nx0+", T= "+(nx0 + chart_data.ts_initial.ts - chart_data.ts_initial.ts0x)+str4+str5+str2+str3);
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
				let tm_ms_up201 = performance.now();
				console.log("Drag xdiff= "+(x - ms_dn_pos[0])+", rel pxl from left= "+xdiff1)
				reset_minx_maxx(gcanvas_args[idx][6], gcanvas_args[idx][7], gcanvas_args[idx][8], gcanvas_args[idx][9]);
				let tm_ms_up202 = performance.now();
				//chart_redraw("cnvs_mouseup"); // this seems to be unnecessary. Already done by zoom_2_new
				let tm_ms_up203 = performance.now();
				console.log(sprintf("__mem: ms_up: tm201-tm1= %.2f, tm202-201= %.2f, tm203-202= %.2f",
							tm_ms_up201 - tm_ms_up1,
							tm_ms_up202-tm_ms_up201,
							tm_ms_up203-tm_ms_up202));
			}
			let tm_ms_up2 = performance.now();
			console.log(sprintf("__mem: ms_up: tm= %.2f", tm_ms_up2 - tm_ms_up1));
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
		if (typeof zoom_to_new_xrange.prev === 'undefined' ||
			zoom_to_new_xrange.invocation_num != can_shape.invocation_num) {
			zoom_to_new_xrange.prev = {};
			zoom_to_new_xrange.invocation_num = can_shape.invocation_num;
		}
		let tm_00 = performance.now();
		let idx = chrt_idx;
    		//console.log("pan x0= "+x0+", x1= "+x1);
		let xdiff = +0.5 * (maxx - minx);
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
		// get rid of noise in computation. We only have nanosec resolution so don't compare more than 9 decimal pts
		let x0s = x0.toFixed(9);
		let x1s = x1.toFixed(9);
		x0 = parseFloat(x0s);
		x1 = parseFloat(x1s);
		if (x1 <= x0) {
			ck_if_need_to_set_zoom_redrawn_cntr("zm_to_nw_a");
			return;
		}
		//console.log("for 04, x1= "+x1);
		if (x0 == minx && x1 == maxx && minx == chart_data.x_range.min && maxx == chart_data.x_range.max) {
			// nothing to do, zoomed out to max;
			ck_if_need_to_set_zoom_redrawn_cntr("zm_to_nw_b");
			return;
		}
		if (update_gsync_zoom) {
			set_gsync_zoom(x0, x1, chart_data.file_tag, chart_data.ts_initial.ts);
		}
		//console.log("zoom: x0: "+x0+",x1= "+x1+", abs x0="+(x0+chart_data.ts_initial.ts)+",ax1="+(x1+chart_data.ts_initial.ts));
		gcanvas_args[idx][6] = x0;
		gcanvas_args[idx][7] = x1;
		reset_minx_maxx(gcanvas_args[idx][6], gcanvas_args[idx][7], gcanvas_args[idx][8], gcanvas_args[idx][9]);
		if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
			console.log(sprintf("__mem: zoom_2_new: x0= %.9f, x1= %.9f, prv: x0= %s x1= %s",
					x0, x1, zoom_to_new_xrange.prev.x0, zoom_to_new_xrange.prev.x1));
		}
		if (typeof zoom_to_new_xrange.prev.x0 !== 'undefined' &&
			zoom_to_new_xrange.prev.x0 == x0 && zoom_to_new_xrange.prev.x1 == x1) {
			//console.log("skip zoom_2_new x0= "+x0+", x1= "+x1);
			ck_if_need_to_set_zoom_redrawn_cntr("zm_to_nw_c");
			return;
		}
		if (typeof zoom_to_new_xrange.prev.x0 === 'undefined') {
			zoom_to_new_xrange.prev = {x0:x0, x1:x1};
		}
		zoom_to_new_xrange.prev = {x0:x0, x1:x1};
		let tm_0 = performance.now();
		//console.log("zoom: x0: "+x0+",x1= "+x1+", abs x0="+(x0+chart_data.ts_initial.ts)+",ax1="+(x1+chart_data.ts_initial.ts));
		chart_redraw("zoom_2_new");
		let tm_1 = performance.now();
		if (chart_data.chart_tag == "SYSCALL_OUTSTANDING_CHART") {
			console.log(sprintf("__mem: __end: zoom_to_new_xrange redraw tm= %.2f tot_tm= %.2f invo= %d",
						tm_1 - tm_0, tm_1 - tm_00, zoom_to_new_xrange.invocation_num));
		}
		//console.log("ch["+chrt_idx+"].rdrw tm= "+tm_diff_str(tm_1-tm_0, 6, 'secs'));
	}
	function fmt_val(val) {
		let val_str;
		if (chart_data.y_fmt != "") {
			val_str = vsprintf(chart_data.y_fmt, [val]);
		} else {
			if (typeof val === 'undefined') {
				return "";
			}
			val_str = val.toFixed(y_axis_decimals);
		}
		return val_str;
	}

	function can_hover(x, y, xscale, yscale)
	{
		/*
		if (chart_data.chart_tag == "PCT_BUSY_BY_CPU") {
			console.log("in hover for "+chart_data.chart_tag);
		}
		*/
		let mtch = -1;
		let rw = -1, row = -1;
		let fnd = -1;
		let fnd_list = [];
		if (ch_type == CH_TYPE_LINE) {
			for (let i = 0; i < lkup.length; i++) {
				if (lkup_use_linearSearch[i]) {
					fnd = linearSearch(lkup[i], x, y, xscale, yscale, compare_in_box);
				} else {
					fnd = binarySearch(lkup[i], x, y, xscale, yscale, compare_in_box);
				}
				//let xs = sprintf("row= %d, x= %f, y= %f, fnd= %s, bx= %s", i, x, y, fnd, (fnd < 0 ? "" : lkup[i][fnd]));
				//console.log(xs);
				if (fnd >= 0) {
					row = i;
					if (tot_line.evt_str.length > 0 && typeof tot_line.subcat_rng_idx[row] !== 'undefined') {
						//console.log("got tot_line");
						fnd_list.push({fnd:fnd, row:row});
						//let use_e = event_lkup[row];
						//let use_s = event_list[use_e].idx;
						//console.log(sprintf("tot_line fnd= %s, use_e= %s, row= %d", fnd, use_e, row));
					} else {
						if (ch_options.overlapping_ranges_within_area) {
							continue;
						}
						let lk = lkup[i][fnd];
						let shape_idx= lk[4];
						let cpt= chart_data.myshapes[shape_idx].ival[IVAL_CPT];
						let use_it = true;
						if (cpt >= 0 && typeof chart_data.proc_arr[cpt] !== 'undefined') {
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
							if (typeof use_e === 'undefined') {
								console.log("prob event_lkup["+row+"] not def. i= "+i+", nm= "+nm+", cpt= "+cpt);
								use_it = false;
							} else {
								//use_s = event_list[use_e].idx;
								//console.log("cpt= "+cpt+", evt_sel= "+event_select[use_s]+", nm= "+nm+", elk= "+use_e);
								//console.log(event_list[use_e]);
								if (cpt >= 0 && typeof event_select[use_s] !== 'undefined' && event_select[use_s][1] == 'hide') {
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
		} else if (ch_type == CH_TYPE_STACKED) {
			for (let i = 0; i < lkup.length; i++) {
				//console.log("stckd i="+i);
				fnd = binarySearch(lkup[i], x, y, xscale, yscale, compare_in_stack);
				if (fnd < 0) {
					fnd = binarySearch(lkup[i], x-2, y, xscale, yscale, compare_in_stack);
					if (fnd < 0) {
					fnd = binarySearch(lkup[i], x+2, y, xscale, yscale, compare_in_stack);
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
							if (cpt >= 0 && typeof event_select[use_s] !== 'undefined' && event_select[use_s][1] == 'hide') {
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
			rw = binarySearch(ylkup, y, x, xscale, yscale, compare_in_rect);
			if (rw < 0) {
				clearToolTipText(mytooltip);
				current_tooltip_text = "";
				current_tooltip_shape = -1;
				return {};
			}
			row = ylkup[rw][4];
			fnd = binarySearch(lkup[row], x, y, xscale, yscale, compare_in_rect);
			if (fnd < 0) {
				fnd = binarySearch(lkup[row], x-1, y, xscale, yscale, compare_in_rect);
				if (fnd < 0) {
					fnd = binarySearch(lkup[row], x+1, y, xscale, yscale, compare_in_rect);
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
			let tot_line_lkup_list = [];
			if (tot_line.evt_str.length > 0 && typeof tot_line.subcat_rng_idx[row] !== 'undefined') {
				//console.log(sprintf("tot_line row= %s, val= %s", row, tot_line.subcat_rng_idx[row]));
				x0  = tot_line.xarray[shape_idx];
				x1  = tot_line.xarray[shape_idx+1];
				let sci = tot_line.subcat_rng_idx[row];
				val = tot_line.yarray[sci][shape_idx];
				tot_line_lkup_list.push(tot_line.lkup[sci][shape_idx]);
				if (fnd_list.length > 1) {
					for (let k=1; k < fnd_list.length; k++) {
						let lk2 = lkup[fnd_list[k].row][fnd_list[k].fnd];
						let shape_idx2= lk2[4];
						let sci = tot_line.subcat_rng_idx[fnd_list[k].row];
						tot_line_lkup_list.push(tot_line.lkup[sci][shape_idx2]);
					}
				}
				cpu = -1;
			} else {
				x0 = chart_data.myshapes[shape_idx].pts[PTS_X0];
				x1 = chart_data.myshapes[shape_idx].pts[PTS_X1];
				cpu = chart_data.myshapes[shape_idx].ival[IVAL_CPU];
				if (ch_type == CH_TYPE_STACKED) {
					val = chart_data.myshapes[shape_idx].pts[PTS_Y1] -
						chart_data.myshapes[shape_idx].pts[PTS_Y0];
				} else {
					val = chart_data.myshapes[shape_idx].pts[PTS_Y1];
				}
				cpt= chart_data.myshapes[shape_idx].ival[IVAL_CPT];
				if (cpt >= 0 && typeof chart_data.proc_arr[cpt] !== 'undefined' && chart_data.proc_arr[cpt].tid > -1) {
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
			if (typeof inserted_value !== 'undefined') {
				x0 = lk[6];
				x1 = lk[7];
				val = inserted_value;
			}
			let str = "at T= "+sprintf("%.6f", x0);
			let lines_done=0;
			if (x0 != x1 || ch_type == CH_TYPE_STACKED) {
				// so we're doing a rectangle
				let tm_dff = tm_diff_str(x1-x0, 6, 'secs');
				str += "-"+sprintf("%.6f", x1)+", dura="+tm_dff+", proc= "+nm+", cpu= "+cpu;
				{
					let x1 = 1.0 * x;
					let xdiff1= +(x1 - xPadding) / (px_wide - xPadding);
					let nx1 = +minx+(xdiff1 * (maxx - minx));
					let abs_ts = (nx1 + chart_data.ts_initial.ts - chart_data.ts_initial.ts0x);
					let str5 = get_phase_indx(abs_ts, false);
					if (str5 != "") {
					//console.log(sprintf("x=  %f, nx1= %f, df= %f abs_ts= %f, str= %s", x, nx1, xdiff1, abs_ts, str5));
					str += "<br>Phase= "+str5;
					}
				}
				if (ch_type == CH_TYPE_LINE || ch_type == CH_TYPE_STACKED) {
					str += ",<br>"+chart_data.y_by_var+"="+chart_data.subcat_rng[row].cat_text;
					let val_str = fmt_val(val);
					str += ", "+chart_data.y_label+"="+val_str;
					lines_done=0;
					for (let k=0; k < fnd_list.length; k++) {
						let lk2 = lkup[fnd_list[k].row][fnd_list[k].fnd];
						let shape_idx2= lk2[4];
						if (tot_line.evt_str.length > 0 && typeof tot_line.subcat_rng_idx[fnd_list[k].row] !== 'undefined') {
							let myx0  = sprintf("%.6f", tot_line.xarray[shape_idx2]);
							let myx1  = sprintf("%.6f", tot_line.xarray[shape_idx2+1]);
							//let myx1  = tot_line.xarray[shape_idx2+1];
							let sci = tot_line.subcat_rng_idx[fnd_list[k].row];
							let val = tot_line.yarray[sci][shape_idx2];
							//str += "<br>fnd_lst_len= "+fnd_list.length;
							str += "<br>x="+myx0+"-"+myx1+","+chart_data.y_by_var+"="+chart_data.subcat_rng[fnd_list[k].row].cat_text;
							if (false) 
							{
							let sc_idx = tot_line.subcat_rng_arr[sci];
							let el_idx = tot_line.event_list_idx[sci];
							let fe_idx = event_list[el_idx].idx;
							let cat_txt = chart_data.subcat_rng[sc_idx].cat_text;
							console.log(sprintf("lgnd: tot_line[%d]: sc_idx= %d, el_idx= %d, fe_idx= %d, evt= %s, row= %d",
										sci, sc_idx, el_idx, fe_idx, cat_txt, fnd_list[k].row));
							}
							let val_str = fmt_val(val);
							str += ", "+chart_data.y_label+"="+val_str;
						} else {
							let myx0 = chart_data.myshapes[shape_idx2].pts[PTS_X0];
							str += "<br>x="+myx0+","+chart_data.y_by_var+"="+chart_data.subcat_rng[fnd_list[k].row].cat_text;
							let val;
							if (ch_type == CH_TYPE_STACKED) {
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
					if (tot_line_lkup_list.length > 0) {
						str += sprintf("<br>%d area(s) in this interval: show up to 10 values: ", tot_line_lkup_list.length);
						for (let kk=0; kk < tot_line_lkup_list.length; kk++) {
							if (tot_line_lkup_list[kk].length == 0) {
								continue;
							}
							let cat_text_init;
							let val_init, nm_init, all_same_nms= true, all_same_values = true, all_same_areas= true;
							lines_done=0;
							let show_max = 10;
							let area_is_tot_line = true;
							for (let k=0; k < tot_line_lkup_list[kk].length; k++) {
								let val;
								let shape_idx2= tot_line_lkup_list[kk][k];
								let nmi = chart_data.myshapes[shape_idx2].ival[IVAL_CAT]
								let cat_txt = chart_data.subcat_rng[nmi].cat_text;
								if (typeof chart_data.subcat_rng[nmi].is_tot_line === 'undefined') {
									 area_is_tot_line = false;
									 //break;
								}
								if (ch_type == CH_TYPE_STACKED) {
									val = chart_data.myshapes[shape_idx2].pts[PTS_Y1] -
										chart_data.myshapes[shape_idx2].pts[PTS_Y0];
								} else {
									val = chart_data.myshapes[shape_idx2].pts[PTS_Y1];
								}
								let cpt2= chart_data.myshapes[shape_idx2].ival[IVAL_CPT];
								nm = null;
								if (cpt2 >= 0 && typeof chart_data.proc_arr[cpt2] !== 'undefined' && chart_data.proc_arr[cpt2].tid > -1) {
									nm = chart_data.proc_arr[cpt2].comm+" "+chart_data.proc_arr[cpt2].pid+"/"+chart_data.proc_arr[cpt2].tid;
								}
								if (k == 0) {
									val_init = val;
									cat_text_init = cat_txt;
									nm_init = nm;
								}
								if (val_init != val) {
									all_same_values = false;
								}
								if (nm_init != nm) {
									all_same_nms = false;
								}
								if (cat_text_init != cat_txt) {
									all_same_areas = false;
								}
								if (!all_same_areas && !all_same_values && !all_same_nms) {
									break;
								}
								if (k > show_max) {
									break;
								}
							}
							//if (!area_is_tot_line && ch_options.overlapping_ranges_within_area)
							if (!area_is_tot_line) {
								//continue;
							}
							let cma="";
							if (tot_line_lkup_list[kk].length < show_max) {
								show_max = tot_line_lkup_list[kk].length;
							}
							if (all_same_areas || all_same_values || all_same_nms) {
								str += sprintf("<br> details below:");
							}
							if (all_same_areas) {
								str += ","+chart_data.y_by_var+"="+cat_text_init;
							}
							if (all_same_values && typeof val_init !== 'undefined') {
								let val_str = fmt_val(val_init);
								str += ", all "+chart_data.y_label+"="+val_str;
							}
							if (all_same_nms) {
								str += ", all procs="+nm_init;
							}
							str += sprintf(" (lines %d of %d): ", show_max, tot_line_lkup_list[kk].length);
							for (let k=0; k < tot_line_lkup_list[kk].length; k++) {
								let shape_idx2= tot_line_lkup_list[kk][k];
								let myx0 = chart_data.myshapes[shape_idx2].pts[PTS_X0];
								let myx1 = chart_data.myshapes[shape_idx2].pts[PTS_X1];
								if (all_same_areas && all_same_values && all_same_nms) {
									let kend = tot_line_lkup_list[kk].length-1;
									if (kend > show_max) {
										kend = show_max;
									}
									shape_idx2= tot_line_lkup_list[kk][kend];
									myx1 = chart_data.myshapes[shape_idx2].pts[PTS_X1];
									str += "<br>x="+myx0+"-"+myx1+", all same value, proc and area";
									break;
								}
								let nmi = chart_data.myshapes[shape_idx2].ival[IVAL_CAT]
								let cat_txt = chart_data.subcat_rng[nmi].cat_text;
								str += "<br>x="+myx0+"-"+myx1;
								if (!all_same_areas) {
									str += ","+chart_data.y_by_var+"="+cat_txt;
								}
								if (!all_same_values) {
									let val;
									if (ch_type == CH_TYPE_STACKED) {
										val = chart_data.myshapes[shape_idx2].pts[PTS_Y1] -
											chart_data.myshapes[shape_idx2].pts[PTS_Y0];
									} else {
										val = chart_data.myshapes[shape_idx2].pts[PTS_Y1];
									}
									let val_str = fmt_val(val);
									str += ", "+chart_data.y_label+"="+val_str;
								}
								let cpt2= chart_data.myshapes[shape_idx2].ival[IVAL_CPT];
								if (cpt2 >= 0 && typeof chart_data.proc_arr[cpt2] !== 'undefined' && chart_data.proc_arr[cpt2].tid > -1) {
									let nm = chart_data.proc_arr[cpt2].comm+" "+chart_data.proc_arr[cpt2].pid+"/"+chart_data.proc_arr[cpt2].tid;
									if (nm != cat_txt) {
										if (!all_same_nms) {
											str += ", proc= "+nm;
										}
									}
								}
								lines_done++;
								if (lines_done > show_max) {
									break;
								}
							}
						}
					}
				}
				if (ch_type == CH_TYPE_STACKED) {
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
					let kbeg = fnd - 100;
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
			if (typeof chart_data.myshapes[shape_idx] !== 'undefined' &&
				typeof chart_data.myshapes[shape_idx].txtidx !== 'undefined') {
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
		return {str2:current_tooltip_text};
	}

	//console.log("mycanvas.width= "+mycanvas.width);
	mycanvas.onmousemove = function(e) {
		if ( typeof can_shape.hvr_prv_x === 'undefined' ) {
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
				can_hover(x, y, 1.0, 1.0);
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
	if ( typeof standaloneJob.data === 'undefined' ) {
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
		if (g_cpu_diagram_flds !== null) {
			console.log("++++g_charts_done bef= "+g_charts_done.cntr);
			let jj=0;
			function myDelay () {           //  create a loop function
				setTimeout(function () {    //  call a 3s setTimeout when the loop is called
					//console.log('jj= '+jj);          //  your code here
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

function draw_text_w_bk(ctx, str, font, font_hi, x, y, sector)
{
    ctx.save();
    ctx.font = font;
    ctx.textBaseline = 'top';
	ctx.textAlign = "left";
    ctx.fillStyle = 'white';
	let sector_add = 0;
	if (sector == 1 && typeof g_cpu_diagram_flds.sectors !== 'undefined' &&
		g_cpu_diagram_flds.sectors.length > 1 &&
		typeof g_cpu_diagram_flds.sectors[1].y_offset !== 'undefined') {
		sector_add = g_cpu_diagram_flds.sectors[1].y_offset;
		//console.log(sprintf("draw_text_w_bk: sector= %d, add= %f", sector, sector_add));
	}
    var width = ctx.measureText(str).width;
    ctx.fillRect(x, y+sector_add, width, font_hi);
    ctx.fillStyle = 'black';
    ctx.fillText(str, x, y+sector_add);
    ctx.restore();
}

function draw_svg_header(lp, xbeg, xend, gen_jtxt, svg_scale_ratio, verbose)
{
	let strm = get_phase_indx(xbeg + 0.5*(xend-xbeg), verbose);
	let strb = get_phase_indx(xbeg, verbose);
	let stre = get_phase_indx(xend, verbose);
	let str5 = "no phase";
	if (strb != stre) {
		str5 = strb + " - " + stre;
	} else {
		str5 = strm;
	}
	let ok = true;
	if (typeof lp === 'undefined') {
		lp = 0;
		ok = false;
	}
	if (typeof xbeg === 'undefined') {
		xbeg = -1.0;
		ok = false;
	}
	if (typeof xend === 'undefined') {
		xend = -1.0;
		ok = false;
	}
	let cma = ", ";
	let ret = {};
	let str = sprintf("lp= %d; phase: %s; T.abs_beg= %.3f; T.abs_end= %.3f;", lp, str5, xbeg, xend);
	ret.str = str;
	if (!ok) {
		return ret;
	}
	if (gen_jtxt) {
		//g_cpu_diagram_canvas.json_text += ', "lp":'+lp;
		g_cpu_diagram_canvas.json_text += ', "phase0":"'+strb+'"';
		g_cpu_diagram_canvas.json_text += ', "phase1":"'+stre+'"';
		g_cpu_diagram_canvas.json_text += ', "t.abs_beg":'+xbeg;
		g_cpu_diagram_canvas.json_text += ', "t.abs_end":'+xend;
	}
	ret.ph0 = strb;
	ret.ph1 = stre;
	ret.tm0 = xbeg;
	ret.tm1 = xend;

	let font_sz = 20;
	let font = font_sz + 'px Arial';
	draw_text_w_bk(g_cpu_diagram_canvas_ctx, str, font, font_sz, 0, 0, 0);
	font_sz = 12;
	font = font_sz + 'px Arial';
	for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
		if (g_cpu_diagram_flds.cpu_diagram_fields[j].chart == "__TEXT_BLOCK__") {
			let x0 = g_cpu_diagram_flds.cpu_diagram_fields[j].fld.x;
			let y0 = g_cpu_diagram_flds.cpu_diagram_fields[j].fld.y;
			let p2 = svg_scale_ratio.cpu_diag.xlate(x0, y0, true);
			let x, y;
			x = p2[0];
			y = p2[1];
			if (false) {
				console.log(sprintf("svg: hdr[%d] pt(%.2f, %.2f)->(%.2f, %.2f), window.devicePixelRatio= %.3f",
						j, x0,y0,x,y, window.devicePixelRatio));
			}
			let ymx = g_cpu_diagram_canvas.height;
			let fmxy = g_cpu_diagram_flds.cpu_diagram_hdr.max_y;
			//console.log(sprintf("draw_svg_header: sector= %d, x= %.3f, y= %.3f", 1, x, y));
			draw_text_w_bk(g_cpu_diagram_canvas_ctx, "phase beg: "+strb, font, font_sz, x, y, 1);
			draw_text_w_bk(g_cpu_diagram_canvas_ctx, "phase end: "+stre, font, font_sz, x, y+font_sz, 1);
			let str_beg = sprintf("phase beg tm_abs: %.3f", xbeg);
			let str_end = sprintf("phase end tm_abs: %.3f", xend);
			draw_text_w_bk(g_cpu_diagram_canvas_ctx, str_beg, font, font_sz, x, y+2*font_sz, 1);
			draw_text_w_bk(g_cpu_diagram_canvas_ctx, str_end, font, font_sz, x, y+3*font_sz, 1);
			break;
		}
	}
	return ret;
}

function get_cpu_busy_divisions()
{
	let cd = null;
	for (let i=0; i < gjson.chart_data.length; i++) {
		if (gjson.chart_data[i].chart_tag == "PCT_BUSY_BY_SYSTEM") {
			cd = gjson.chart_data[i];
			break;
		}
	}
	if (cd !== null) {
		let xdff = cd.x_range.max - cd.x_range.min;
		let j = xdff / 0.01;
		j = Math.ceil(j) + 1;
		console.log(sprintf("cpu_busy rng: dff= %.3f, %.3f - %.3f, div= %d", 
			xdff, cd.x_range.min, cd.x_range.max, j));
		return j;
	}
	return -1;
}

function draw_svg_footer(xmx, ymx, copyright)
{
	if (typeof copyright === 'undefined' || copyright === null) {
		return "";
	}

	let str = sprintf("%s; See %s;", copyright.text, copyright.website);
	let font_sz = 12;
	let font = font_sz + 'px Arial';
	ymx = g_cpu_diagram_canvas.height;
	let y = ymx - font_sz;
	draw_text_w_bk(g_cpu_diagram_canvas_ctx, str, font, font_sz, 0, y, 0);
	str = sprintf("SVG from %s", copyright.SVG_URL);
	y = ymx - 2*font_sz;
	draw_text_w_bk(g_cpu_diagram_canvas_ctx, str, font, font_sz, 0, y, 0);
	return str;
}

	function myblob(can_ele, str5)
	{
		can_ele.toBlob(function(blob) {
			let reader = new FileReader();
			reader.readAsDataURL(blob); 
			reader.onloadend = function() {
				let base64data = reader.result;                
				webSocket.send(str5+base64data);
				let buf_amt = webSocket.bufferedAmount;
				//console.log("ws sent bytes= "+base64data.length+", buf_amt= "+buf_amt);
			}
		}, 'image/png', 1.0);
	}

function get_mem_usage(str)
{
	// might need to start chrome with --enable-precise-memory-info
	// otalJSHeapSize: 1038017615, usedJSHeapSize: 703481759, jsHeapSizeLimit: 2162163712}
	let mem = window.performance.memory;
	if (typeof mem === 'undefined' || mem === null) {
		// MS edge browser seems to not support this.
		return;
	}
	// chrome/firebox/brave seem to support it.
	let fctr = 1.0/(1024.0 * 1024.0);
	console.log(sprintf("%s usedHeap: %.3f, totHeap= %.3f MBs",
				str, fctr * mem.usedJSHeapSize, fctr * mem.totalJSHeapSize));
}

async function start_charts() {
	let tm_beg = performance.now();
	let tm_now = performance.now();
	console.log("can_shape beg. elap ms= " + (tm_now-tm_beg));
	chart_divs.length = 0;
	let ch_did_it = [];
	get_mem_usage("__mem: at 0: ");
	for (let i=0; i < gjson.chart_data.length; i++) {
		chart_did_image.push(null);
		ch_did_it.push(false);
	}
	if (typeof gjson.phase !== 'undefined') {
		// replace fixed strings in phase text. the fixed strings don't provide much info and make the phase string unnecessarily long
		let str_arr = [
			["tracing_mark_write: begin phase ", ""],
			["tracing_mark_write: end phase ", ""],
			["tracing_mark_write: ", ""]
		   ];
		for (let i=0; i < gjson.phase.length; i++) {
			for (let j=0; j < str_arr.length; j++) {
				let idx = gjson.phase[i].text.indexOf(str_arr[j][0]);
				if (idx >= 0) {
					gjson.phase[i].text = gjson.phase[i].text.replace(str_arr[j][0], str_arr[j][1]);
				}
			}
		}
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
	let save_g_tot_line_division_max = g_tot_line_divisions.max;
	let by_phase = 0;
	if (typeof gjson.by_phase !== 'undefined') {
		if (gjson.by_phase[0] == "1") {
			by_phase = 1;
			let j = get_cpu_busy_divisions();
			if (g_tot_line_divisions.max < j) {
				g_tot_line_divisions.max = j;
			}
		}
	}
	let chrts_started_max = chrts_started;
	chrts_started = 0;
	//myBarMove(0.0, chrts_started_max);
	if (typeof gjson.pixels_high_default !== 'undefined' && gjson.pixels_high_default >= 100) {
		gpixels_high_default = gjson.pixels_high_default;
	}
	if (typeof gjson.phase !== 'undefined') {
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
				if (typeof gjson.chart_data[i].marker_ymin !== 'undefined') {
					zoom_y0 = parseFloat(gjson.chart_data[i].marker_ymin);
				}
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
	get_mem_usage("__mem: at 1: ");
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
	console.log("ph_step_int: ", gjson.ph_step_int);
	console.log("ph_image: ", gjson.ph_image);

	let ph_step = 0.0, ph_loops = -1;
	if (typeof gjson.ph_step_int !== 'undefined') {
		ph_step = gjson.ph_step_int[0];
		if (gjson.ph_step_int.length > 1) {
			ph_loops= gjson.ph_step_int[1];
		} else {
			ph_loops = -1;
		}
	}
	let skip_phases_with_string = null;
	if (typeof gjson.skip_phases_with_string !== 'undefined') {
		skip_phases_with_string = gjson.skip_phases_with_string[0];
	}
	if (typeof gjson.by_phase !== 'undefined') {
		if (gjson.by_phase[0] == "1") {
			by_phase = 1;
			//let j = get_cpu_busy_divisions();
		}
	}
	let did_ck_phase = 0;
	let phobj = {lp:0, step:ph_step, lp_max:ph_loops, by_phase:by_phase, skip_phases_with_string:skip_phases_with_string};

	let ibeg = -1, iend= -1;
	let divisions = phobj.lp_max;
	if (typeof gjson.phase !== 'undefined') {
		for (let i=0; i < gjson.phase.length; i++) {
			if (gjson.phase[i].zoom_to == 1) {
				ibeg = i;
			}
			if (gjson.phase[i].zoom_end == 1) {
				iend = i;
			}
		}
	}
	console.log(sprintf("ibeg= %d, iend= %d", ibeg, iend));
	let xbeg = -1, xend= -1;

	// calc how many divisions to put between the begin and end
	if (phobj.by_phase == 1) {
		let rc = -1;
		// so both --phase0 and --phase1 entered
		[xbeg, xend, rc] = get_by_phase_beg_end(ibeg);

		phobj.lp_max = 1+iend-ibeg; // need to do + 1 since end condition is lp < mx
		divisions = 100;
		let try_divi = get_cpu_busy_divisions();
		if (divisions < try_divi) {
			divisions = try_divi;
		}
		g_tot_line_divisions.max = divisions;
		console.log(sprintf("++cpu_busy by_phase xbeg= %.3f, xend= %.3f, step= %.5f, divisions= %d",
					xbeg, xend, phobj.step, divisions));
		g_tot_line_divisions.max = divisions;
	} else if (ibeg != -1 && iend == -1) {
		// so --phase0 entered and --phase1 not entered
		xend  = gjson.phase[ibeg].ts_abs;
		xbeg  = xend;
		xbeg -= gjson.phase[ibeg].dura;
		if (phobj.lp_max > 0 && phobj.step > 0) {
			xend = xbeg + phobj.lp_max * phobj.step;
		}
		if (phobj.lp_max > 0) {
			divisions = phobj.lp_max;
			if (phobj.step <= 0.0) {
				phobj.step = (xend - xbeg)/divisions;
			}
		} else if (phobj.step > 0) {
			divisions = Math.ceil((xend - xbeg)/phobj.step);
		}
		console.log(sprintf("xbeg= %.3f, xend= %.3f, step= %.5f, divisions= %d",
					xbeg, xend, phobj.step, divisions));
	} else if (ibeg != -1 && iend != -1) {
		// so both --phase0 and --phase1 entered
		xbeg  = gjson.phase[ibeg].ts_abs;
		xbeg -= gjson.phase[ibeg].dura;
		xend  = gjson.phase[iend].ts_abs;
		if (phobj.lp_max > 0) {
			divisions = phobj.lp_max;
			if (phobj.step <= 0.0) {
				phobj.step = (xend - xbeg)/divisions;
			}
		} else if (phobj.step > 0) {
			divisions = Math.ceil((xend - xbeg)/phobj.step);
		}
		console.log(sprintf("xbeg= %.3f, xend= %.3f, step= %.5f, divisions= %d",
					xbeg, xend, phobj.step, divisions));
	}

	if (phobj.by_phase != 1) {
		if (divisions > 0) {
			g_tot_line_divisions.max = divisions;
		}

		phobj.lp_max = divisions;
	}
	if (g_cpu_diagram_canvas !== null) {
		g_cpu_diagram_canvas.json_text = null;
	}

	let image_whch_txt_init = -200;

	function reset_OS_view_image_ready()
	{
		if (g_cpu_diagram_flds === null) {
			return;
		}
		for (let j=0; j < gjson.chart_data.length; j++) {
			if (typeof gjson.chart_data[j].fl_image_ready !== 'undefined') {
				gjson.chart_data[j].fl_image_ready = false;
				gjson.chart_data[j].image_whch_txt = image_whch_txt_init;
			}
			if (typeof gjson.chart_data[j].image_ready !== 'undefined') {
				gjson.chart_data[j].image_ready = false;
				gjson.chart_data[j].image_whch_txt = image_whch_txt_init;
			}
		}
	}
	function display_OS_view_image_ready()
	{
		if (g_cpu_diagram_flds === null) {
			return;
		}
		for (let j=0; j < gjson.chart_data.length; j++) {
			let wt = gjson.chart_data[j].image_whch_txt;
			if (typeof wt === 'undefined') {
				wt = -100;
			}
			if (typeof gjson.chart_data[j].fl_image_ready !== 'undefined') {
				console.log("fl_rdy= "+gjson.chart_data[j].fl_image_ready+", wt= "+wt);
				console.log(sprintf("ch[%d] fl_rdy= %s, wh_txt= %s", j, gjson.chart_data[j].fl_image_ready, wt));
			}
			if (typeof gjson.chart_data[j].image_ready !== 'undefined') {
				console.log(sprintf("ch[%d] rdy= %s, wh_txt= %d", j, gjson.chart_data[j].image_ready, wt));
			}
		}
	}

	let skip_obj = {skip_if_idle:true, idle_cur:-1.0, idle_skip_if_less_than:50.0, lp:-1, lp_prev:-2, idle_prev:51.0};

	function ck_if_skip_due_to_idle(lp, so)
	{
		so.skip_if_idle = false;
		let idle_dvals = g_cpu_diagram_flds.lkup_dvals_for_chart_tag["PCT_BUSY_BY_SYSTEM"];
		if (typeof idle_dvals === 'undefined') {
			return false;
		} else {
			let jidx = idle_dvals[0]; // can only handle 1 indx for now. If chart appears > 1 in has*.flds then broken
			let mx = g_cpu_diagram_flds.cpu_diagram_fields[jidx].data_val_arr.length;
			if (mx > 0) {
				//console.log(sprintf("tl.yarr2.sz= %d, j.sz= %d",
				//	g_cpu_diagram_flds.cpu_diagram_fields[jidx].tot_line.yarray2.length,
				//	g_cpu_diagram_flds.cpu_diagram_fields[jidx].tot_line.yarray2[0].length));
				so.skip_if_idle = true;
				//idle_cur = g_cpu_diagram_flds.cpu_diagram_fields[jidx].data_val_arr[0];
				so.idle_cur = g_cpu_diagram_flds.cpu_diagram_fields[jidx].tot_line.yarray[0][lp];
				if (lp > 0) {
					so.idle_prev = g_cpu_diagram_flds.cpu_diagram_fields[jidx].tot_line.yarray[0][lp-1];
				}
				//console.log(sprintf("lp= %d, %%idle= %.3f%%, ax0= %.3f, ax1= %.3f",
				//			lp, so.idle_cur, gsync_zoom_last_zoom.abs_x0, gsync_zoom_last_zoom.abs_x1));
			}
		}
		let do_skip = false;
		if (so.skip_if_idle && so.idle_cur < so.idle_skip_if_less_than && so.idle_prev < so.idle_skip_if_less_than) {
			do_skip = true;
		}
		return do_skip;
	}

	function send_blob_backend(po, xbeg, from)
	{
		let x0, x1, str, rc=-1;
		if (xbeg == -1.0) {
			str = "lp= -2";
			rc = 0;
		} else {
			if (po.by_phase == 1) {
				[x0, x1, rc] = get_by_phase_beg_end(po.lp);
				console.log(sprintf("__blob: phs[%d], x0= %.3f, x1= %.3f", po.lp, x0, x1));
				let ret_obj = draw_svg_header(po.lp, x0, x1, false, g_svg_scale_ratio, false);
				str = ret_obj.str;
			} else {
				rc = 0;
				x0 = xbeg + po.step*po.lp;
				x1 = xbeg + po.step*(po.lp+1);
				let ret_obj = draw_svg_header(po.lp, x0, x1, false, g_svg_scale_ratio, false);
				str = ret_obj.str;
			}
		}
		if (rc != -1) {
			console.log(sprintf("send_blob by_phase= %d: lp= %d, from= %s str= %s", po.by_phase, po.lp, from, str));
			myblob(g_cpu_diagram_canvas, "image,"+str+",imagedata:");
		} else {
			console.log(sprintf("skipping invalid phase %d", po.lp));
		}
	}

	/*
	 *  We need to wait for the initial draw of the charts to complete before we do anything else.
	 *  Then we need to check if a phase is selected,
	 *A) if so, set typ= wait_for_zoom_to_phase and redraw all the charts to the zoom interval
	 *   wait for the redraws to finish so the tot_lines values are recomputed for the zoom interval for the cpu_diagram
	 *   then set typ= zoom_cpu_diag and wait for redraw of the cpu_diagram
	 *   then, if requested, save the image and send to server
	 *   then, if more zooms are requested then go to A)
	 */
	let cd_cpu_busy = null;
	let cpu_busy_tot_line = null;
	if (false) {
		if (g_cpu_diagram_flds !== null) {
			for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
				if (g_cpu_diagram_flds.cpu_diagram_fields[j].chart_tag == "PCT_BUSY_BY_SYSTEM") {
					cd_cpu_busy = g_cpu_diagram_flds.cpu_diagram_fields[j];
					break;
				}
			}
		}
	}

	let jj = 0, jjmax_reset_value = 100, jjmax = jjmax_reset_value;
	let myDelay_timeout = 5000;
	let last_draw_svg = -100;

	function myDelay (lkfor_max, po)
	{           //  create a loop function
		setTimeout(function () {    //  call a 3s setTimeout when the loop is called
			jj++;                     //  increment the counter
			console.log('====jj= '+jj+', charts_done= '+g_charts_done.cntr+", redrw= "+
				gsync_zoom_redrawn_charts.cntr+", lkfor= "+lkfor_max.cntr+", typ= "+lkfor_max.typ);
			if (jj < jjmax && (lkfor_max.typ == "wait_for_initial_draw" && lkfor_max.cntr < gjson.chart_data.length)) {
				myDelay(lkfor_max, po);             //  ..  again which will trigger another 
			} else if (jj < jjmax && (lkfor_max.typ == "wait_for_zoom_to_phase" && 
				 (lkfor_max.cntr < gsync_zoom_redrawn_charts.need_to_redraw))) {
				myDelay(lkfor_max, po);             //  ..  again which will trigger another 
			} else if (jj < jjmax && (lkfor_max.typ == "wait_for_zoom_to_phase" && 
				 (lkfor_max.cntr >= gsync_zoom_redrawn_charts.need_to_redraw && !got_all_OS_view_images(po.lp, image_whch_txt_init)))) {
					 console.log("by_phase: mxc do draw_svg lp= "+po.lp);
				if (po.lp != last_draw_svg) {
					g_cpu_diagram_draw_svg([], -1, po.lp);
					last_draw_svg = po.lp;
				}
				myDelay(lkfor_max, po);             //  ..  again which will trigger another 
			} else {
				if ((lkfor_max.typ == "wait_for_zoom_to_phase" &&
						(lkfor_max.cntr < gsync_zoom_redrawn_charts.need_to_redraw || !got_all_OS_view_images(po.lp, image_whch_txt_init))
					)) {
					gsync_zoom_linked = false;
				}
				if (jj >= jjmax) {
					console.log("problem with myDelay rtn: jj >= jjmax("+jjmax+"). typ= "+lkfor_max.typ);
					display_OS_view_image_ready();
					return;
				}
				console.log(sprintf("++++typ= %s, gs_zm_rd.cntr= %d, cpu_diag_rd= %d, lp= %d, lpmx= %d ax0= %.3f ax1= %.3f %%",
					lkfor_max.typ, gsync_zoom_redrawn_charts.cntr, gsync_zoom_redrawn_charts.cpu_diag_redrawn, po.lp, po.lp_max,
							gsync_zoom_last_zoom.abs_x0, gsync_zoom_last_zoom.abs_x1));
				did_ck_phase = 0;
				let bump_lp = 0;
				jj = 0;
				if (po.lp < po.lp_max) {
					if (bump_lp > 0 && g_cpu_diagram_canvas !== null && po.by_phase != 1) {
						let do_skip = ck_if_skip_due_to_idle(po.lp, skip_obj);
						if (!do_skip) {
							send_blob_backend(po, xbeg, "mydelay");
						} else {
							console.log("skip due to 2+ idles; lp= "+po.lp);
						}
					}
					if ((po.lp) < po.lp_max) {
						po.lp = po.lp + bump_lp;
						if ((po.lp) < po.lp_max) {
							ck_phase(lkfor_max, po);
						}
					}
				}
				if (po.lp_max == -1) {
					console.log(sprintf("+++1 lp= %d, lp_max= %d", po.lp, po.lp_max));
					if (po.lp == 0) {
						po.lp = po.lp + 1;
						lkfor_max.typ == "wait_for_zoom_to_phase";
						myDelay(lkfor_max, po);             //  ..  again which will trigger another 
					}
					if (po.lp == 1) {
						console.log(sprintf("+++2 lp= %d, lp_max= %d", po.lp, po.lp_max));
						//send_svg_tbl();
					}
				}
			}
		}, myDelay_timeout, lkfor_max, po)
	}
	g_charts_done.typ = "wait_for_initial_draw";
	myDelay(g_charts_done, phobj);
	console.log("---- did myDelay(g_charts_done)");
	console.log("---- try setting divisions= "+phobj.lp_max);

	function get_by_phase_beg_end(phs)
	{
		let x0, x1;
		if (typeof gjson.phase === 'undefined' || typeof gjson.phase[phs] === 'undefined') {
			console.log(sprintf("__phase issue. phs= %s, gjson.phase.len= %d", phs, gjson.phase.length));
			return [0.0, 0.0, -1];
		}
		if (phs < 0) {
			x0  = gjson.phase[0].ts_abs;
			x0 -= gjson.phase[0].dura;
			x0 += 0.0001; // make sure it resolves to inside phase
			let last = gjson.phase.length - 1;
			x1  = gjson.phase[last].ts_abs - 0.0001; // make sure it resolves to inside phase
			console.log(sprintf("get_by_phase_beg_end: phs= %d, x0[%d]= %.3f, x1[%d]= %.3f",
						phs, 0, x0, last, x1));
		} else {
			x0  = gjson.phase[phs].ts_abs;
			x0 -= gjson.phase[phs].dura;
			x0 += 0.0001; // make sure it resolves to inside phase
			x1  = gjson.phase[phs].ts_abs - 0.0001; // make sure it resolves to inside phase
		}
		return [x0, x1, phs];
	}
	function set_zoom_xbeg_xend(po)
	{
		gsync_zoom_redrawn_charts.cntr = 0;
		gsync_zoom_last_zoom.chrt_idx = -1;
		gsync_zoom_last_zoom.x0 = -1;
		gsync_zoom_last_zoom.x1 = -1;
		if (po.by_phase == 1) {
			let rc = -1;
			[xbeg, xend, rc] = get_by_phase_beg_end(po.lp);
			//if (skip_obj.skip_if_idle && po.lp > -1 && cd_cpu_busy !== null && cpu_busy_tot_line.xdiff > 0.0) 
			if (rc >= 0 && po.lp > -1 && cd_cpu_busy !== null && cpu_busy_tot_line.xdiff > 0.0) {
				let cdi = cd_cpu_busy.chrt_idx;
				let ts   = gjson.chart_data[cdi].ts_initial.ts;
				let ts0x = gjson.chart_data[cdi].ts_initial.ts0x;
				let x0 = cpu_busy_tot_line.sv_xarray[0];
				let xb = xbeg - x0;
				let i = Math.floor(xb/cpu_busy_tot_line.xdiff);
				let xe = xend - x0;
				let ie = Math.floor(xe/cpu_busy_tot_line.xdiff);
				if (i >= 0 && ie > (i+1)) {
					let got_busy = false;
					let chg_rng  = false;
					let tval = cpu_busy_tot_line.sv_yarray[i];
					//console.log(sprintf("++ck phs[%d] x0= %.3f ts= %.3f, 0x= %.3f, o=%.3f-%.3f, i= %d, ie= %d, len= %d, b0= %f, skp_if= %f", po.lp,
					//	x0, ts, ts0x, xbeg, xend, i, ie, cpu_busy_tot_line.sv_yarray.length, tval, skip_obj.idle_skip_if_less_than));
					for (let j=i; j < ie; j++) {
						let x_cur    = cpu_busy_tot_line.sv_xarray[j];
						let busy_cur = cpu_busy_tot_line.sv_yarray[j];
						let busy_nxt = cpu_busy_tot_line.sv_yarray[j+1];
						if (busy_cur > skip_obj.idle_skip_if_less_than) {
							got_busy = true;
						}
						if (got_busy && busy_cur < skip_obj.idle_skip_if_less_than && busy_nxt < skip_obj.idle_skip_if_less_than) {
							console.log(sprintf("++cut phs[%d] o=%.3f-%.3f n=%.3f-%3f", po.lp, xbeg, xend, xbeg, x_cur));
							xend = x_cur;
							chg_rng = true;
							break;
						}
					}
					if (!chg_rng) {
					console.log(sprintf("++nocut phs[%d] o=%.3f-%.3f", po.lp, xbeg, xend));
					}
				}
			}
		}
		gsync_zoom_last_zoom.abs_x0 = xbeg;
		gsync_zoom_last_zoom.abs_x1 = xend;
	}

	function ck_skip_phases_with_string(po)
	{
		if (typeof po.skip_phases_with_string === 'undefined' ||
			po.skip_phases_with_string === null) {
				return;
		}
		for (; po.lp < po.lp_max; po.lp++) {
			if (gjson.phase[po.lp].text.indexOf(po.skip_phases_with_string) < 0) {
				break;
			}
			console.log(sprintf("skipping phase[%d]= %s", po.lp, gjson.phase[po.lp].text));
		}
	}

	function ck_phase(lkfor_max, po)
	{
		jj=0;
		jjmax=jjmax_reset_value;
		function do_draws(po)
		{
			// if we don't put in a sleep then the websocket.send's never get started and we bomb off (eventually)
			if (po.by_phase == 1) {
				console.log("....error....");
				return;
			}
			setTimeout(function () {
				if (po.lp == 0) {
					g_cpu_diagram_draw_svg([], -2, -1);
					send_blob_backend(po, -1.0, "do_draws0");
				}
				if (po.lp < po.lp_max) {
					let do_skip = ck_if_skip_due_to_idle(po.lp, skip_obj);
					if (!do_skip) {
						g_cpu_diagram_draw_svg([], po.lp, -1);
						send_blob_backend(po, xbeg, "do_draws1");
					}
					po.lp++;
					do_draws(po);
				} else {
					console.log(sprintf("typ= %s, lp= %d, lpmx= %d", lkfor_max.typ, po.lp, po.lp_max));
					if (po.lp == po.lp_max) {
						g_tot_line_divisions.max = save_g_tot_line_division_max;
						g_cpu_diagram_draw_svg([], -1, -1);
						po.lp++;
						do_draws(po);
						g_cpu_diagram_canvas.json_text += "]}";
						webSocket.send("json_table="+g_cpu_diagram_canvas.json_text);
						console.log("sent json_table to server");
						//console.log(g_cpu_diagram_canvas.json_text);
					} 
				}
			}, 100, po);
		}
		if (lkfor_max.typ == "wait_for_zoom_to_phase" && g_cpu_diagram_canvas !== null) {
			if (po.by_phase == 1) {
				console.log(sprintf("---- by_phase: lp= %d, lpmax= %d, phs_max= %d", po.lp, po.lp_max, gjson.phase.length));
				if (po.lp != last_draw_svg) {
					g_cpu_diagram_draw_svg([], -1, po.lp);
					last_draw_svg = po.lp;
				}
				send_blob_backend(po, xbeg, "ck_phase0");
				reset_OS_view_image_ready();
				myDelay_timeout = 1000;
				jj = 0;
				jjmax = jjmax_reset_value;
				let old_typ = gsync_zoom_redrawn_charts.typ;
				if (po.lp < gjson.phase.length) {
					po.lp = po.lp + 1;
					ck_skip_phases_with_string(po);
					if (po.lp < po.lp_max) {
						set_zoom_xbeg_xend(po);
						set_zoom_all_charts(-1, gjson.phase[0].file_tag, po.lp, "ck_phase 0");
						console.log(sprintf("---- by_phase: lp= %d, wait...", po.lp));
						myDelay(gsync_zoom_redrawn_charts, po);
						if (cd_cpu_busy !== null) {
							console.log("++cpu_busy.yarray.len= ", cpu_busy_tot_line.sv_yarray.length);
						}
					} else {
						g_cpu_diagram_canvas.json_text += "]}";
						webSocket.send("json_table="+g_cpu_diagram_canvas.json_text);
						console.log("sent json_table to server");
						// this is the end of the loop
						// below is an attempt to zoom back out and redraw at the end... maybe get it working later
						/*
						let pot = po;
						pot.lp = -1;
						set_zoom_xbeg_xend(pot);
						set_zoom_all_charts(-1, gjson.phase[0].file_tag, po.lp);
						console.log(sprintf("---- by_phase: lp= %d, start wait...", pot.lp));
						*/
					}
				}
			} else {
				do_draws(po);
			}
		} else if (lkfor_max.typ == "wait_for_initial_draw" && typeof gjson.phase !== 'undefined') {
			let old_typ = gsync_zoom_redrawn_charts.typ;
			if (po.by_phase == 1) {
				g_tot_line_divisions.max = save_g_tot_line_division_max;
				if (cd_cpu_busy !== null) {
					console.log("++cpu_busy.yarray.len= ",  cd_cpu_busy.tot_line.yarray[0].length);
					if (cpu_busy_tot_line === null) {
						cpu_busy_tot_line = {};
						cpu_busy_tot_line.sv_xarray  = [];
						cpu_busy_tot_line.sv_yarray  = [];
						let cdi = cd_cpu_busy.chrt_idx;
						let ts   = gjson.chart_data[cdi].ts_initial.ts;
						let ts0x = gjson.chart_data[cdi].ts_initial.ts0x;
						let xoff = ts - ts0x;
						for (let ii=0; ii < cd_cpu_busy.tot_line.yarray[0].length; ii++) {
							cpu_busy_tot_line.sv_xarray.push(cd_cpu_busy.tot_line.xarray[ii]+xoff);
							cpu_busy_tot_line.sv_yarray.push(cd_cpu_busy.tot_line.yarray[0][ii]);
						}
						//console.log(cpu_busy_tot_line.sv_yarray);
						cpu_busy_tot_line.xdiff = cpu_busy_tot_line.sv_xarray[1] - cpu_busy_tot_line.sv_xarray[0];
					}
				}
				
				if (-1 != last_draw_svg) {
					g_cpu_diagram_draw_svg([], -2, -1);
					last_draw_svg = -1;
				}
				send_blob_backend(po, -1.0, "ck_phase1");
				po.lp = 0;
			}
			set_zoom_xbeg_xend(po);
			console.log(sprintf("===zoom x0= %s, x1= %s lp= %d %%",
						gsync_zoom_last_zoom.abs_x0, gsync_zoom_last_zoom.abs_x1, po.lp));
			set_zoom_all_charts(-1, gjson.phase[0].file_tag, -1, "ck_phase 1");
			gsync_zoom_redrawn_charts.typ = "wait_for_zoom_to_phase";
			myDelay(gsync_zoom_redrawn_charts, po);
			console.log("---- did myDelay("+old_typ+")");
		}
	}
	get_mem_usage("__mem: at 2: ");

	return;
}


function get_phase_indx(ts_abs, verbose)
{
	if (typeof gjson.phase === 'undefined') {
		return "";
	}
	let smallest_diff_idx = -1, smallest_diff=0;
	for (let i=0; i < gjson.phase.length; i++) {
		let dura   = gjson.phase[i].dura;
		if (dura < 1.0e-5) {
			continue;
		}
		let tm_beg = gjson.phase[i].ts_abs - dura;
		let tm_end = gjson.phase[i].ts_abs;
		let dist0 = Math.abs(tm_beg - ts_abs);
		let dist1 = Math.abs(tm_end - ts_abs);
		if (smallest_diff_idx == -1) {
			smallest_diff = dist0;
			smallest_diff_idx = i;
		}
		if (smallest_diff > dist0) {
			//console.log(sprintf("near0[%d]: beg= %.3f, ts= %.3f, end= %.3f sm= %.2f, d0= %.2f",
			//			i, tm_beg, ts_abs, tm_end, smallest_diff, dist0));
			smallest_diff = dist0;
			smallest_diff_idx = i;
		}
		if (smallest_diff > dist1) {
			//console.log(sprintf("near1[%d]: beg= %.3f, ts= %.3f, end= %.3f sm= %.2f, d1= %.2f",
			//			i, tm_beg, ts_abs, tm_end, smallest_diff, dist1));
			smallest_diff = dist1;
			smallest_diff_idx = i;
		}
		if (verbose) {
			console.log(sprintf("ck phase[%d]: beg= %.3f, ts= %.3f, end= %.3f nearest= %d, diff= %.2f",
						i, tm_beg, ts_abs, tm_end, smallest_diff_idx, smallest_diff));
		}
		if (tm_beg <= ts_abs && ts_abs <= tm_end) {
			//console.log(sprintf("phase= %s ts_abs= %f", gjson.phase[i].text, ts_abs));
			return gjson.phase[i].text;
		}
	}
	if (smallest_diff_idx != -1) {
		let i = smallest_diff_idx;
		let dura   = gjson.phase[i].dura;
		let tm_beg = gjson.phase[i].ts_abs - dura;
		let tm_end = gjson.phase[i].ts_abs;
		let dist0 = tm_beg - ts_abs;
		let dist1 = tm_end - ts_abs;
		let adist0 = Math.abs(tm_beg - ts_abs);
		let adist1 = Math.abs(tm_end - ts_abs);
		let b_or_e = "";
		let ck_dst;
		//console.log(sprintf("ck_nr[%d], tm_beg= %.3f, ts= %.3f, tm_end= %.3f dura= %.3f", i, tm_beg, ts_abs, tm_end, dura));
		//console.log(sprintf("ck_nr[%d], d0= %.3f, d1= %.3f, a0= %.3f, a1= %.3f", i, dist0, dist1, adist0, adist1));
		if (adist0 < adist1) {
			b_or_e = "beg ";
			ck_dst = dist0;
		} else {
			b_or_e = "end ";
			ck_dst = dist1;
		}
		if (ck_dst < -0.001) {
			b_or_e = "aft " + b_or_e;
		} else if (ck_dst > 0.001) {
			b_or_e = "bef " + b_or_e;
		}
		return b_or_e + gjson.phase[i].text;
	}
	// now handle non-overlapping cases
	return "";
}

function parse_chart_data(ch_data_str)
{
	let str = vsprintf("%.1f MBs", [ch_data_str.length/(1024.0*1024.0)]);
	var tm_beg = performance.now();
	//document.title = "bef JSON "+str+",tm="+tm_diff_str(0.001*(tm_beg-g_tm_beg), 1, "secs");
	if (typeof gjson.chart_data !== 'undefined') {
		gjson.chart_data.push(JSON.parse(ch_data_str));
	}
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
	if(typeof webSocket !== 'undefined' && webSocket.readyState !== WebSocket.CLOSED){
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
	if (text == "html2canvas") {
		console.log("you entered "+text);
		//try_doing_html2canvas();
	} else {
		webSocket.send(text);
	}
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
	if (mymodal === null) {
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
	if (typeof gjson === 'undefined' || typeof gjson.chart_data === 'undefined') {
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
	if (typeof svg_gs === 'undefined' || svg_gs === null || svg_gs.length == 0) {
		return;
	}
	let rect_prev = -1;
	let rects=0;
	let texts=0;
	let svg_xmax = +svg_gs[0].getAttributeNS(null, 'width');
	let svg_ymax = +svg_gs[0].getAttributeNS(null, 'height');
	let svg_name = svg_gs[0].getAttributeNS(null, 'sodipodi:docname');
	let svg_ver = svg_gs[0].getAttributeNS(null, 'version');
	console.log("svg_name= "+svg_name+', ver= '+svg_ver);

	let y_shift = 0;
	if (typeof g_cpu_diagram_flds.sectors !== 'undefined' && g_cpu_diagram_flds.sectors.length > 1 &&
		typeof g_cpu_diagram_flds.sectors[1] !== 'undefined' && 
		typeof g_cpu_diagram_flds.sectors[1].sector !== 'undefined' && 
			   g_cpu_diagram_flds.sectors[1].sector == 1 && 
		typeof g_cpu_diagram_flds.sectors[1].y_offset !== 'undefined' &&
		       g_cpu_diagram_flds.sectors[1].y_offset > 0) {
		y_shift = g_cpu_diagram_flds.sectors[1].y_offset;
	}

	// begin find end-points
	let lkfor_id = null;
	//lkfor_id = "tspan3027";
	//lkfor_id = "tspan1293";

	function svg_parse_path(path, xfrm, rect_prev, shapes, gxf_arr) {
		let xf = path.getAttributeNS(null, 'transform');
		if (xf == '' || xf === null) {
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
		if (xf == '' || xf === null) {
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
		if (tstyle === null) {
			xft = "";
		}
		let xft = txt.getAttributeNS(null, 'transform');
		let gxf_len = gxf.length;
		if (xft == '' || xft === null) {
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
				if ((typeof style === 'undefined' || style === null) && tstyle !== null) {
					style = tstyle;
				}
				let id   = txt.children[k].getAttributeNS(null, 'id');
				let t    = txt.children[k].innerHTML;
				let xf   = txt.children[k].getAttributeNS(null, 'transform');
				if (xf == '' || xf === null) {
					xf = "";
				}
				if (lkfor_id !== null && id == lkfor_id) {
					console.log("--got id= "+id);
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
		let id   = ge.getAttributeNS(null, 'id');
		let xfrm = ge.getAttributeNS(null, 'transform');
		if (xfrm == '' || xfrm === null) {
			xfrm = "";
		} else {
			if (xfrm.length > 7 && xfrm.substr(0,7) == "matrix(") {
				//console.log("xfrm= "+xfrm);
			}
		}
		if (lkfor_id !== null && id == lkfor_id) {
			console.log(sprintf("__g id= %s, xfrm= %s", id, xfrm));
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
			if (lkfor_id !== null && id == lkfor_id) {
				console.log("__pop: ",gxf_arr[gxf_arr.length-1]);
			}
			gxf_arr.pop();
		}
		/*
		*/
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
	//console.log(sprintf("svg: width= %s, height= %s", svg_xmax, svg_ymax));
	let hvr_clr = 'mysvg';
	let win_sz = get_win_wide_high();
	let px_wide = win_sz.width - 30;
	g_svg_scale_ratio = {svg_xratio:px_wide/svg_xmax, cpu_diag:null};
	if (typeof gjson.img_wxh_pxls !== 'undefined' && gjson.img_wxh_pxls.length > 0) {
		let wxh = gjson.img_wxh_pxls[0];
		let idx = wxh.indexOf("x");
		if (idx == -1) {
			idx = wxh.indexOf(",");
		}
		if (idx == -1) {
			console.log(sprintf("didn't find ',' nor 'x' in img_wxh_pxls str= '%s'. Ignoring --img_wxh_pxls option",
						wxh));
		} else {
			let w = parseInt(wxh.substr(0, idx));
			let h = parseInt(wxh.substr(idx+1));
			console.log(sprintf("got img_wxh_pxls= %s, w= %d h= %d", wxh, w, h));
			px_wide = w;
			g_svg_scale_ratio.svg_xratio = px_wide/svg_xmax;
		}
	}
	let px_high = svg_ymax/svg_xmax * px_wide;
	//console.log(sprintf("svg: width= %s, height= %s, px_wide= %.2f, px_high= %.2f, g_svg_scale_ratio.svg_xratio= %.3f, win_sz.width= %.3f",
	//			svg_xmax, svg_ymax, px_wide, px_high, g_svg_scale_ratio.svg_xratio, win_sz.width));
	//console.log(sprintf("svg: wi= %s, de.cw= %s, cw= %s",
	//	window.innerWidth, document.documentElement.clientWidth, document.body.clientWidth));
	let myhvr_clr = document.getElementById(hvr_clr);
	let str = '<div id="tbl_'+hvr_clr+'"></div><div class="tooltip"><canvas id="canvas_'+hvr_clr+'" width="'+(px_wide)+'" height="'+(px_high+y_shift)+'" style="border:1px solid #000000;"><span class="tooltiptext" id="tooltip_'+hvr_clr+'"></span></div>';
	myhvr_clr.innerHTML = str;
	let mycanvas  = document.getElementById('canvas_'+hvr_clr);
	let mytooltip = document.getElementById("tooltip_"+hvr_clr);
	let mytable   = document.getElementById("tbl_"+hvr_clr);
	let tt_prv_x=-2, tt_prv_y=-2;
	let ctx = mycanvas.getContext("2d");
	g_cpu_diagram_canvas = mycanvas;
	g_cpu_diagram_canvas_ctx = ctx;
	let xlate_rng = {state:0, xmin:null, ymin:null, xmax:null, ymax:null};

	{
	g_svg_scale_ratio.cpu_diag = {shift:0.0, scale:0.0};
	let p0 = xlate(ctx, 0.0, 0.0, 0, svg_xmax, 0, svg_ymax, null, 1);
	g_svg_scale_ratio.cpu_diag.shift = p0;
	//console.log(sprintf("svg: shift 0.0,0.0-> x= %.2f, y= %.2f", p0[0], p0[1]));
	let p1 = xlate(ctx, 1.0, 1.0, 0, svg_xmax, 0, svg_ymax, null, 1);
	//console.log(sprintf("svg: scale 1.0,1.0-> x= %.2f, y= %.2f", p1[0]-p0[0], p1[1]-p0[1]));
	g_svg_scale_ratio.cpu_diag.scale = [p1[0]-p0[0], p1[1]-p0[1]];
	}
	g_svg_scale_ratio.cpu_diag.xmax = svg_xmax;
	g_svg_scale_ratio.cpu_diag.ymax = svg_ymax;
	g_svg_scale_ratio.cpu_diag.xlate_base = xlate;
	g_svg_scale_ratio.cpu_diag.ctx = ctx;
	g_svg_scale_ratio.cpu_diag.xlate = function(x_in, y_in, do_shift) {
		let fmxx = g_cpu_diagram_flds.cpu_diagram_hdr.max_x;
		let fmxy = g_cpu_diagram_flds.cpu_diagram_hdr.max_y;
		let x2 = g_svg_scale_ratio.cpu_diag.xmax * x_in/fmxx;
		let y2 = g_svg_scale_ratio.cpu_diag.ymax * y_in/fmxy;
		let p2 = xlate(g_svg_scale_ratio.cpu_diag.ctx, x2, y2, 0, g_svg_scale_ratio.cpu_diag.xmax, 0, g_svg_scale_ratio.cpu_diag.ymax, null, 1);
		p2[1] = p2[1] - (do_shift ? g_svg_scale_ratio.cpu_diag.shift[1] : 0.0);
		return p2;
	}

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

	function decode_xform(xin, yin, xform, verbose, xorig, yorig) {
		let xout = xin;
		let yout = yin;
		if (xform.length > 7 && xform.substr(0,7) == "rotate(") {
			let xf = xform.substr(7, xform.length-8);
			if (verbose) {
				console.log("--rotate angle= "+xf);
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
				console.log(sprintf("--scl: xo/n= '%f/%f', y='%f/%f'", xout, xn, yout, yn));
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
				console.log(sprintf("--xlt: %s, orig:x= %.3f,y= %.3f)", xform, xorig, yorig));
				console.log(sprintf("--xlt: xo/n= '%f/%f', y='%f/%f'", xout, xn, yout, yn));
			}
			xout = xn;
			yout = yn;
		}
		if (xform.length > 7 && xform.substr(0,7) == "matrix(") {
			// matrix(1,0,0,1.1860756,223.40923,319.50555)
			// newX = a * oldX + c * oldY + e
			// newY = b * oldX + d * oldY + f
			let xf = xform.substr(7, xform.length-8);
			let scl = xf.split(",");
			if (verbose) {
				console.log(sprintf("--got matrix.len %d, %s", scl.length, xform));
			}
			for (let i=0; i < scl.length; i++) {
				scl[i] = parseFloat(scl[i]);
			}
			let xn= scl[0] * xout + scl[2] * yout + scl[4];
			let yn= scl[1] * xout + scl[3] * yout + scl[5];
			if (verbose) {
				console.log(sprintf("--xlt: xo/n= '%f/%f', y='%f/%f'", xout, xn, yout, yn));
			}
			xout = xn;
			yout = yn;
			/*
			*/
		}
		return [xout, yout];
	}


	function xlate(tctx, xin, yin, uminx, umaxx, uminy, umaxy, shape, sector) {
		//let xout = Math.trunc(px_wide * (xin - uminx)/ (umaxx - uminx));
		//let yout = Math.trunc(px_high * (yin - uminy)/ (umaxy - uminy));
		let xout = xin;
		let yout = yin;
		let verbose = 0;
		if (shape !== null) {
			let id = shape.id;
			if (lkfor_id !== null && id == lkfor_id) {
				let xfct = (xin - uminx)/ (umaxx - uminx);
				console.log(sprintf("--xlate beg id= %s, xin= %.3f, xout= %.3f, umnx= %.3f, umxx= %.3f, xfct= %.3f",
							id, xin, xout, uminx, umaxx, xfct));
				verbose = 1;
			}
			if (shape.xf1 != "") {
				[xout, yout] = decode_xform(xout, yout, shape.xf1, verbose, xin, yin);
			}
			if (shape.xf0.length > 0) {
				for (let i=shape.xf0.length-1; i >=0; i--) {
					[xout, yout] = decode_xform(xout, yout, shape.xf0[i], verbose, xin, yin);
				}
			}
			if (lkfor_id !== null && id == lkfor_id) {
				console.log(sprintf("--xlate end id= %s, xin= %.3f, xout= %.3f", id, xin, xout));
				verbose = 1;
			}
		}
		xout = px_wide * (xout - uminx)/ (umaxx - uminx);
		yout = px_high * (yout - uminy)/ (umaxy - uminy);
		if (xlate_rng.state == 0) {
			if (xlate_rng.xmin === null || xlate_rng.xmin > xout) {
				xlate_rng.xmin = xout;
			}
			if (xlate_rng.ymin === null || xlate_rng.ymin > yout) {
				xlate_rng.ymin = yout;
			}
			if (xlate_rng.xmax === null || xlate_rng.xmax < xout) {
				xlate_rng.xmax = xout;
			}
			if (xlate_rng.ymax === null || xlate_rng.ymax < yout) {
				xlate_rng.ymax = yout;
			}
		} else {
			//xout -= xlate_rng.xmin;
			//yout -= xlate_rng.ymin;
		}

		if (sector == 1) {
			yout += y_shift;
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


	function build_poly() {
		let pt  = {x:0, y:0};
		for (let i=0; i < shapes.length; i++) {
			if (shapes[i].typ != 'path') {
				continue;
			}
			let vrb = 0;
			if (lkfor_id !== null && shapes[i].id == lkfor_id) {
				vrb = 1;
			}
			shapes[i].poly = [];
			for (let j=0; j < shapes[i].cmds.length; j++) {
				pt.x = shapes[i].cmds[j].pt_a[0].x;
				pt.y = shapes[i].cmds[j].pt_a[0].y;
				let p0 = xlate(ctx, pt.x, pt.y, 0, svg_xmax, 0, svg_ymax, shapes[i], 1);
				shapes[i].poly.push({x:p0[0], y:p0[1]});
				if (shapes[i].cmds[j].cmd == 'C' || shapes[i].cmds[j].cmd == 'c') {
					p0 = xlate(ctx, shapes[i].cmds[j].pt_a[2].x, shapes[i].cmds[j].pt_a[2].y, 0, svg_xmax, 0, svg_ymax, shapes[i], 1);
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
			let p0 = xlate(ctx, x, y, 0, svg_xmax, 0, svg_ymax, shape, 1);
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
			if (typeof shapes[i].poly[j].arrow_extended !== 'undefined') {
				add_path_connections(i, 'path', poly, shapes[i].poly[j].x, shapes[i].poly[j].y);
			} else if (typeof shapes[i].poly[j].endpoint_line !== 'undefined') {
				add_path_connections(i, 'path', poly, shapes[i].poly[j].endpoint_line.x0, shapes[i].poly[j].endpoint_line.y0);
			}
		}
	}

	function add_path_connections(i, typ, poly, x, y) {
		let arr3 = find_overlap(x, y);
		let arr2 = [];
		for (let j=0; j < arr3.length; j++) {
			arr2.push(arr3[j].j);
		}
		if (typeof poly.im_connected_to === 'undefined') {
			poly.im_connected_to = [];
		}
		if (lkfor_id !== null && shapes[i].id == lkfor_id) {
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
					if (typeof shapes[arr2[k]].is_connected_to_me === 'undefined') {
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

	function drawBarChart(ctx, data, colors, grf_def_idx, do_legend) {
		let px_x, px_y_hdr;
		function xlate_bar(x1, y1) {
			let fmxx = g_cpu_diagram_flds.cpu_diagram_hdr.max_x;
			let fmxy = g_cpu_diagram_flds.cpu_diagram_hdr.max_y;
			let x2 = svg_xmax * x1/fmxx;
			let y2 = svg_ymax * y1/fmxy;
			return xlate(ctx, x2, y2, 0, svg_xmax, 0, svg_ymax, null, 1);
		}
		let x1 = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].fld.x;
		let y1 = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].fld.y;
		[px_x, px_y_hdr] = xlate_bar(x1, y1);
		let tpx = 13;
		let tstr = sprintf("%dpx sans-serif", tpx);
		let hdr_ftr  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.hdr_ftr_high;
		let px_y     = px_y_hdr + hdr_ftr;
		let px_wide  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.wide;
		let px_high  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.high;
		let typ      = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.typ;
		let grf_name = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.nm;
		let balance  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance;
		let balance_hdr  = null;
		let balance_chrt = null;
		let bal_x   = -1;
		let bal_y   = -1;
		let bal_max_value = null;
		if (typeof balance !== 'undefined' && balance !== null) {
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.hdr !== 'undefined') {
				balance_hdr  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.hdr;
			}
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.tot_chart !== 'undefined') {
				balance_chrt = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.tot_chart;
			}
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.x !== 'undefined') {
				bal_x = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.x;
				bal_y = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.y;
				[bal_x, bal_y] = xlate_bar(bal_x, bal_y);
			}
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.max_value !== 'undefined') {
				bal_max_value = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.max_value;
			}
		}
		let x = (px_wide / 2),
			y = (px_high / 2);
		x = y = 0.0;

		let bar_wd = 20.0;

		function sumArray(data) {
			let sum = 0;
			let sum2 = 0;
			if (balance_chrt !== null) {
				for(let i=0; i<data.length; i++) {
					if (data[i].chart_tag == balance_chrt) {
						sum2 = data[i].value;
					} else {
						sum += data[i].value;
					}
				}
			} else {
				for(let i=0; i<data.length; i++) {
					sum += data[i].value;
				}
				sum2 = 100.0;
			}
			return [sum, sum2];
		}

		let total, total_abs;
		let twoPI = 1.0;
  
		function calcHeight(data, index, total) {
			return 100.0 * data[index].value / total;
		}

		g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].bars = [];

		let bal_hdr_str = balance_hdr;

		// bar chart
		for (let ch=0; ch < data.length; ch++) {
			let data_nw = data[ch];
			//console.log(sprintf("data_nw[%d].len= %d", ch, data_nw.length));
			[total, total_abs] = sumArray(data_nw);
			if (bal_max_value !== null) {
				total = bal_max_value;
			}
			let do_bal = false;
			let x2 = ch * bar_wd;
			let arr = [];

			if (ch == 0 || data_nw.length > 1) {
				ctx.font = tstr;
				ctx.fillStyle = 'black';
				let evt_str = grf_name;
				ctx.fillText(evt_str, px_x + x2, px_y_hdr);
				//evt_str = data_nw[0].evt_str;
				//ctx.fillText(evt_str, px_x + x2, px_y+px_high+hdr_ftr);
			}

			let heights = [];
			let curHi=0.0, prvHi = 0.0;
			for(let i=0; i < data_nw.length; i++) {
				if (balance_chrt !== null && data_nw[i].chart_tag == balance_chrt) {
					heights.push({beg:prvHi, end:prvHi});
					curHi = 0.0;
				} else {
					curHi = calcHeight(data_nw, i, total);
					heights.push({beg:prvHi, end:(prvHi+curHi)});
				}
				prvHi += curHi;
			}
			let bar_x0 = px_x;
			let bar_y0 = px_y;
			let bar_x1 = px_wide;
			let bar_y1 = px_high;
			let bar = {x:bar_x0, y:bar_y0, wide:px_wide, high:px_high, arr:[]};
			let bal_slice = -1;
			// bar chart
			for(let i=0; i < data_nw.length; i++) {
				if (balance_chrt !== null && data_nw[i].chart_tag == balance_chrt) {
					bal_slice = i;
				}
				let bar_hi = px_high * (0.01 * (heights[i].end - heights[i].beg));
				bar_x0 = px_x + x2;
				bar_y0 = px_y+px_high;
				bar_x1 = bar_wd;
				bar_y1 = -bar_hi;

				arr.push({value:data_nw[i].value,
					begHeight:(heights[i].beg),
					endHeight:(heights[i].end),
					bar_shape:{x0:bar_x0, y0:bar_y0, wide:bar_x1, high:bar_y1},
					evt_str:data_nw[0].evt_str,
					lbl:data_nw[i].label,
					pct:(heights[i].end - heights[i].beg),
					data_nw_ent:data_nw[i]});
				let leg_num = i;
				if (bal_slice != -1) {
					leg_num -= 1;
				}
				ctx.beginPath();
				if (data_nw.length > 1) {
					ctx.fillStyle = colors[leg_num];
				} else {
					ctx.fillStyle = colors[ch];
				}
				ctx.rect(px_x + x + x2, px_y+y+px_wide, bar_wd, -bar_hi);
				ctx.fill();
				ctx.stroke();
				let tnum = sprintf("%d", ch);
				let twidth = ctx.measureText(tnum).width;
				let angleHalf = heights[i].beg + 0.5 * bar_hi;
				if (do_legend && false && (ch+1) == data.length) {
					ctx.save();
					let atxt = "";
					atxt = sprintf(", ab= %.3f, ae= %.3f ah= %.3f diff= %.1f",
							heights[i].beg, heights[i].end, angleHalf, bar_hi);
					ctx.rect(px_x + px_wide + 10 + x2, px_y + leg_num * (tpx+5), 12, 12);
					ctx.fill();
					ctx.font = tstr;
					let dval = "";
					if (!do_bal) {
						dval = sprintf(": %.3f, ", data_nw[i].value);
					}
					let tvals = "";
					if (data.length == 1) {
						let tspc = "";
						if (data_nw[i].label.length > 0 && 
								(data_nw[i].label.substr(-1) != " " && data_nw[i].label.substr(-1) != ":")) {
							tspc = " ";
						}
						tvals = tspc + dval + sprintf("%.2f%%", 100.0*bar_hi/twoPI);
					}
					let lstr = tnum + " " + data_nw[i].label + tvals;
					let leg_x = px_x + px_wide + 25 + x2;
					let leg_y = px_y +  leg_num * (tpx+5) + 10;
					let do_bkgrnd = false;
					if (do_bkgrnd) {
						ctx.fillStyle = 'white';
						let width = ctx.measureText(lstr).width;
						ctx.fillRect(leg_x, leg_y - tpx + 5, width, tpx);
					}
					ctx.fillStyle = 'black';
					ctx.fillText(lstr, leg_x, leg_y);
					ctx.restore();
				}
			}
			bar.arr = arr;
			g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].bars.push(bar);
		}
		ctx.save();
		ctx.fillStyle = 'black';
		ctx.rect(px_x, px_y, data.length * bar_wd , px_high);
		ctx.stroke();
		let lbl_len_max = 0.0;
		for (let i=0.0; i <= bal_max_value; i += 0.50) {
			let lstr = sprintf("%.2f", i);
			let width = ctx.measureText(lstr).width;
			if (lbl_len_max < width) {
				lbl_len_max = width;
			}
			ctx.fillText(lstr, px_x + data.length * bar_wd + 5, px_y + px_high - px_high * (i/bal_max_value));
		}
		for (let i=0.0; i <= bal_max_value; i += 0.25) {
			let ln_x = px_x + data.length * bar_wd;
			let ln_y = px_y + px_high - px_high * (i/bal_max_value);
			ctx.moveTo(ln_x, ln_y);
			ctx.lineTo(ln_x + 5, ln_y);
			ctx.stroke();
		}
		if (do_legend) {
			ctx.save();
			for (let i=0; i < data.length; i++) {
				let lstr = data[i][0].evt_str;
				let ln_x = px_x + data.length * bar_wd + 2*5 + lbl_len_max;
				let ln_y = px_y + i * (tpx+5);
				ctx.fillStyle = colors[i];
				ctx.fillRect(ln_x, ln_y, 12, 12);
				//ctx.fill();
				ctx.stroke();
				ctx.textBaseline = 'top';
				ctx.fillStyle = 'black';
				ctx.fillText(lstr, ln_x + 15, ln_y);
			}
			ctx.restore();
		}

		if (bal_hdr_str !== null && bal_x != -1 && bal_y != -1) {
			ctx.fillStyle = 'black';
			ctx.fillText(bal_hdr_str, bal_x, bal_y);
		}
	}

	function drawPieChart(ctx, data, colors, grf_def_idx, do_legend) {
		let px_x, px_y_hdr;
		function xlate_pie(x1, y1) {
			let fmxx = g_cpu_diagram_flds.cpu_diagram_hdr.max_x;
			let fmxy = g_cpu_diagram_flds.cpu_diagram_hdr.max_y;
			let x2 = svg_xmax * x1/fmxx;
			let y2 = svg_ymax * y1/fmxy;
			return xlate(ctx, x2, y2, 0, svg_xmax, 0, svg_ymax, null, 1);
		}
		let x1 = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].fld.x;
		let y1 = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].fld.y;
		[px_x, px_y_hdr] = xlate_pie(x1, y1);
		let tpx = 13;
		let tstr = sprintf("%dpx sans-serif", tpx);
		let hdr_ftr  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.hdr_ftr_high;
		let px_y     = px_y_hdr + hdr_ftr;
		let px_wide  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.wide;
		let px_high  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.high;
		let typ      = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.typ;
		let grf_name = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.nm;
		let balance  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance;
		let balance_hdr  = null;
		let balance_chrt = null;
		let bal_x   = -1;
		let bal_y   = -1;
		if (typeof balance !== 'undefined' && balance !== null) {
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.hdr !== 'undefined') {
				balance_hdr  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.hdr;
			}
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.tot_chart !== 'undefined') {
				balance_chrt = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.tot_chart;
			}
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.x !== 'undefined') {
				bal_x = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.x;
				bal_y = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.balance.y;
				[bal_x, bal_y] = xlate_pie(bal_x, bal_y);
			}
		}
		let x = (px_wide / 2),
			y = (px_high / 2);

		function sumArray(data) {
			let sum = 0;
			let sum2 = 0;
			if (balance_chrt !== null) {
				for(let i=0; i<data.length; i++) {
					if (data[i].chart_tag == balance_chrt) {
						sum2 = data[i].value;
					} else {
						sum += data[i].value;
					}
				}
			} else {
				for(let i=0; i<data.length; i++) {
					sum += data[i].value;
				}
				sum2 = 100.0;
			}
			return [sum, sum2];
		}

		let total, total_abs;
		let twoPI = 2.0 * Math.PI;
  
		function calcAngle(data, index, total) {
			return data[index].value / total * twoPI;
		}

		g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].pies = [];

		let bal_hdr_str = balance_hdr;

		for (let ch=0; ch < data.length; ch++) {
			let data_nw = data[ch];
			//console.log(sprintf("data_nw[%d].len= %d", ch, data_nw.length));
			[total, total_abs] = sumArray(data_nw);
			let do_bal = false;
			if (balance_hdr !== null && total < total_abs) {
				let dff = total_abs - total;
				let dff_str = sprintf(' %.1f%%;', dff);
				bal_hdr_str += dff_str;
				data_nw.push({label:balance_hdr, value: dff});
				do_bal = true;
				total = total_abs;
			}
			let x2 = ch * px_wide; 
			let pie = {x:(px_x + x + x2), y:(px_y + y), wide:px_wide, high:px_high, arr:[]};
			let arr = [];
			let rot = 0.5 * Math.PI;

			ctx.font = tstr;
			ctx.fillStyle = 'black';
			let evt_str = grf_name;
			ctx.fillText(evt_str, px_x + x2, px_y_hdr);
			evt_str = data_nw[0].evt_str;
			ctx.fillText(evt_str, px_x + x2, px_y+px_high+hdr_ftr);

			let angles = [];
			let curAngle=0.0, prvAngle = 0.0;
			for(let i=0; i < data_nw.length; i++) {
				if (balance_chrt !== null && data_nw[i].chart_tag == balance_chrt) {
					angles.push({beg:prvAngle, end:prvAngle});
					curAngle = 0.0;
				} else {
					curAngle = calcAngle(data_nw, i, total);
					angles.push({beg:prvAngle, end:(prvAngle+curAngle)});
				}
				prvAngle += curAngle;
			}
			let bal_slice = -1;
			for(let i=0; i < data_nw.length; i++) {
				if (balance_chrt !== null && data_nw[i].chart_tag == balance_chrt) {
					bal_slice = i;
				}
				let diff   = angles[i].end - angles[i].beg;
				let endA = angles[i].end;
				let arcLen = 0.7 * x * diff;

				if (i==0) {
					ctx.moveTo(px_x + x + x2, px_y + y);
					ctx.arc(px_x + x + x2, px_y+y, x*1.2, angles[i].beg-rot, angles[i].beg-rot);
					ctx.stroke();
				}
				arr.push({value:data_nw[i].value,
					begAngle:(angles[i].beg + rot),
					endAngle:(angles[i].end + rot),
					lbl:data_nw[i].label,
					pct:(diff/twoPI),
					data_nw_ent:data_nw[i]});
				let slc_num = i;
				if (bal_slice != -1) {
					slc_num -= 1;
					if (bal_slice == i) {
						continue;
					}
				}
				let leg_num = i;
				if (bal_slice != -1) {
					leg_num -= 1;
				}
				ctx.beginPath();
				ctx.fillStyle = colors[leg_num];
				ctx.moveTo(px_x + x + x2, px_y + y);
				ctx.arc(px_x + x + x2, px_y+y, x, angles[i].beg-rot, angles[i].end-rot);
				ctx.fill();
				ctx.stroke();
				let tnum = sprintf("%d", slc_num);
				let twidth = ctx.measureText(tnum).width;
				let angleHalf = angles[i].beg + 0.5 * diff;
				if (do_legend && (ch+1) == data.length) {
					ctx.save();
					let atxt = "";
					atxt = sprintf(", ab= %.3f, ae= %.3f ah= %.3f diff= %.1f, rot= %.1f",
							angles[i].beg, angles[i].end, angleHalf, diff, rot);
					ctx.rect(px_x + px_wide + 10 + x2, px_y + leg_num * (tpx+5), 12, 12);
					ctx.fill();
					ctx.font = tstr;
					let dval = "";
					if (!do_bal) {
						dval = sprintf(": %.3f, ", data_nw[i].value);
					}
					let tvals = "";
					if (data.length == 1) {
						let tspc = "";
						if (data_nw[i].label.length > 0 && 
								(data_nw[i].label.substr(-1) != " " && data_nw[i].label.substr(-1) != ":")) {
							tspc = " ";
						}
						tvals = tspc + dval + sprintf("%.2f%%", 100.0*diff/twoPI);
					}
					//tvals += atxt;
					let lstr = tnum + " " + data_nw[i].label + tvals;
					let leg_x = px_x + px_wide + 25 + x2;
					let leg_y = px_y +  leg_num * (tpx+5) + 10;
					let do_bkgrnd = false;
					if (do_bkgrnd) {
						ctx.fillStyle = 'white';
						let width = ctx.measureText(lstr).width;
						ctx.fillRect(leg_x, leg_y - tpx + 5, width, tpx);
					}
					ctx.fillStyle = 'black';
					ctx.fillText(lstr, leg_x, leg_y);
					ctx.restore();
				}
				if (arcLen >= (2.0 * twidth)) {
					//ctx.save();
					let xt = 0.7 * x * Math.cos(angleHalf-rot);
					let yt = 0.7 * x * Math.sin(angleHalf-rot);
					let fl = ctx.fillStyle;
					let st = ctx.strokeStyle;
					ctx.fillStyle = 'black';
					ctx.strokeStyle = 'black';
					let mytx = px_x + x + x2 + xt;
					let myty = px_y + y + yt;
					ctx.fillText(tnum, mytx, myty);
					ctx.stroke();
					ctx.fillStyle = fl;
					ctx.strokeStyle = st;
					//ctx.restore();
					//console.log(sprintf("ch= %d, i= %d, twd= %.1f, arclen= %.1f, x= %.1f, y= %.1f, mytx= %.1f, y= %.1f, tnum= %s",
					//	ch, i, twidth, arcLen, xt, yt, mytx, myty, tnum));
				}
			}
			pie.arr = arr;
			g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].pies.push(pie);
		}

		if (bal_hdr_str !== null && bal_x != -1 && bal_y != -1) {
			ctx.fillStyle = 'black';
			ctx.fillText(bal_hdr_str, bal_x, bal_y);
		}
	};


	let draw_svg = function(hilite_arr, whch_txt, subtst) {
		build_poly();
		ctx.clearRect(0, 0, mycanvas.width, mycanvas.height);
		ctx.fillStyle = 'white';
		ctx.fillRect(0, 0, mycanvas.width, mycanvas.height);
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
						let p0 = xlate(ctx, x, y, 0, svg_xmax, 0, svg_ymax, shapes[i], 1);
						x = shapes[i].cmds[j].pt[1].x;
						y = shapes[i].cmds[j].pt[1].y;
						if (typ == 'c') {
							x += pt.x;
							y += pt.y;
						}
						let p1 = xlate(ctx, x, y, 0, svg_xmax, 0, svg_ymax, shapes[i], 1);
						x = shapes[i].cmds[j].pt[2].x;
						y = shapes[i].cmds[j].pt[2].y;
						if (typ == 'c') {
							x += pt.x;
							y += pt.y;
						}
						let p2 = xlate(ctx, x, y, 0, svg_xmax, 0, svg_ymax, shapes[i], 1);
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
					if (lkfor_id !== null && shapes[i].id == lkfor_id) {
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
						if (lkfor_id !== null && shapes[i].id == lkfor_id) {
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
					if (lkfor_id !== null && shapes[i].id == lkfor_id) {
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
				let p0 = xlate(ctx, x0, y0, 0, svg_xmax, 0, svg_ymax, shapes[i], 1);
				let p1 = xlate(ctx, x1, y1, 0, svg_xmax, 0, svg_ymax, shapes[i], 1);
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
				if (fill_clr !== null) {
					ctx.fillStyle = fill_clr;
					ctx.fillRect(p0[0],p0[1], wd, hi);
				}
				if (highlight) {
					strk_clr = 'black';
					ctx.lineWidth = 3;
				}
				if (strk_clr !== null) {
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
				let text_anchor = 'middle';
				let str = shapes[i].text;
				if (str.indexOf('&amp;') >= 0) {
					str = str.replace(/&amp;/g, '&');
				}
				for (let j=0; j < tst.length; j++) {
					if (tst[j].length > 11 && tst[j].substr(0,11) == "text-align:") {
						text_align = tst[j].substr(11);
					}
					if (tst[j].length > 12 && tst[j].substr(0,12) == "text-anchor:") {
						text_anchor = tst[j].substr(12);
					}
				}
				let style = tstyle + ";" + shapes[i].style;
				let nfont_sz = 11;
				let line_hi = nfont_sz;
				let font_sz = sprintf("%dpx", nfont_sz);
				if (typeof style !== 'undefined' && style !== null) {
					let st = style.split(";");
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
				}
				// if we've scaled down the canvas sz from svg sz then font can be too big to fit in box
				nfont_sz *= g_svg_scale_ratio.svg_xratio;
				shapes[i].nfont_sz = nfont_sz;
				let x0 = shapes[i].x;
				let y0 = shapes[i].y;
				if (str == "Stage 2a") {
					console.log(sprintf("__stg2a xy= %.3f,%.3f", x0, y0));
				}
				if (line_hi > 1.0) {
					y0 -= nfont_sz * (line_hi - 1.0);
				}
				if (str == "Stage 2a") {
					console.log(sprintf("__stg2a xy= %.3f,%.3f, nfont_sz= %.3f, line_hi= %.3f", x0, y0, nfont_sz, line_hi));
				}
				//let y0 = shapes[i].y - nfont_sz;
				let p0 = xlate(ctx, x0, y0, 0, svg_xmax, 0, svg_ymax, shapes[i], 1);
				//let p1 = xlate(ctx, x1, y1, 0, svg_xmax, 0, svg_ymax, shapes[i], 1);
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
				if (str == "Stage 2a") {
					console.log(sprintf("__stg2a xy= %.3f,%.3f", x0, y0));
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
						//let p0 = xlate(ctx, x, y, 0, svg_xmax, 0, svg_ymax, shapes[i], 1);
						//ctx.arc(p0[0], p0[1], 5, 0,2*Math.PI);
						//console.log(str_angle);
						shapes[i].poly[j].angle = shapes[i].angles[j];
						shapes[i].is_connector = true;
						add_path_connections(i, 'rect', shapes[i].poly[j], shapes[i].poly[j].arrow_extended.x, shapes[i].poly[j].arrow_extended.y);
						add_path_connections(i, 'path', shapes[i].poly[j], shapes[i].poly[j].x, shapes[i].poly[j].y);
						ctx.stroke();
						ctx.closePath();
						if (lkfor_id !== null && shapes[i].id == lkfor_id) {
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
							if (lkfor_id !== null && shapes[i].id == lkfor_id) {
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
		let txt_tbl = draw_svg_txt_flds(whch_txt, subtst);
		if (txt_tbl.length > 0) {
			// when we are generating phase pngs, the first time through has txt_tbl.len == 0
			for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
				if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].grf_def !== 'undefined') {
					let typ    = g_cpu_diagram_flds.cpu_diagram_fields[j].grf_def.typ;
					let do_leg = false;
					if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].grf_def.legend !== 'undefined' &&
						g_cpu_diagram_flds.cpu_diagram_fields[j].grf_def.legend !== null) {
						do_leg = g_cpu_diagram_flds.cpu_diagram_fields[j].grf_def.legend;
					}
					if (typ == "vbar") {
						let bar_data = build_pie_data(j, whch_txt, txt_tbl);
						//console.log("bar_data", bar_data);
						drawBarChart(ctx, bar_data, g_d3_clrs_c20, j, do_leg);
					}
					if (typ == "pie") {
						let pie_data = build_pie_data(j, whch_txt, txt_tbl);
						drawPieChart(ctx, pie_data, g_d3_clrs_c20, j, do_leg);
					}
				}
			}
		}
		if (typeof g_cpu_diagram_flds.figures !== 'undefined' &&
			g_cpu_diagram_flds.figures.length > 0) {
				console.log(sprintf("by_phase: process_figures, whch_txt= %d", whch_txt));
				process_figures(ctx, whch_txt, subtst);
		}
		if (xlate_rng.state == 0) {
			xlate_rng.state = 1;
			console.log(sprintf("ddd xlate_rng: xmn= %.3f xmx= %.3f, ymn= %.3f, ymx= %.3f",
				xlate_rng.xmin, xlate_rng.xmax, xlate_rng.ymin, xlate_rng.ymax));
		}
	}
	g_cpu_diagram_draw_svg = draw_svg;

	function process_figures(ctx, whch_txt, subtst) {
		let sector = 1;
		ctx.save();
		let last_box_top=null, last_box_bot=null, last_box_left=null, last_box_right=null;
		let last_box_top_px=null, last_box_bot_px=null, last_box_left_px=null, last_box_right_px=null;

		function get_image(ctx, whch_txt, fig_idx, cmd_idx, doing_flame, chart_tag, image_nm, xb, xe, yb, ye) {
			if (g_cpu_diagram_flds === null) {
				return -1;
			}
			let use_j = -1;
			let use_chart_tag = chart_tag;
			//console.log(sprintf("__get_image_nm0= %s, chrt= %s", image_nm, use_chart_tag));
			if (!doing_flame && image_nm !== null && chart_tag === null) {
				for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
					if (chart_tag === null && image_nm !== null && 
						typeof g_cpu_diagram_flds.cpu_diagram_fields[j].save_image_nm !== 'undefined' &&
						g_cpu_diagram_flds.cpu_diagram_fields[j].save_image_nm === image_nm) {
						g_cpu_diagram_flds.cpu_diagram_fields[j].fig_cmd_idx = {fig:fig_idx, cmd:cmd_idx};
						use_chart_tag = g_cpu_diagram_flds.cpu_diagram_fields[j].chart;
						//console.log(sprintf("__get_image_nm1= %s, chrt= %s", image_nm, use_chart_tag));
						break;
					}
				}
			}
			for (let j=0; j < gjson.chart_data.length; j++) {
				if (use_chart_tag !== null && gjson.chart_data[j].chart_tag == use_chart_tag) {
					use_j = j;
					break;
				}
			}
			let img_idx = -1;
			for (let j=0; j < gjson.chart_data.length; j++) {
				if (gjson.chart_data[j].chart_tag != use_chart_tag) {
					continue;
				}
				//console.log(sprintf("__get_image_nm2= %s, chrt= %s fl= %s xb= %.3f", image_nm, use_chart_tag, doing_flame, xb));
				if (doing_flame) {
					//console.log(sprintf("%s fl_img_rdy= %s", chart_tag, gjson.chart_data[j].fl_image_ready));
					if (gjson.chart_data[j].fl_image_ready === true) {
						ctx.drawImage(gjson.chart_data[j].fl_image, xb, yb, xe-xb, ye-yb);
						if (whch_txt >= 0) {
							gjson.chart_data[j].image_whch_txt = whch_txt;
						} else if (subtst >= 0) {
							gjson.chart_data[j].image_whch_txt = subtst;
						} else {
							gjson.chart_data[j].image_whch_txt = subtst;
						}
						//console.log(sprintf("by_phase: set cd[%d].fl whch_txt= %d", j, whch_txt));
						img_idx = j;
					}
				} else {
					//console.log(sprintf("%s img_rdy= %s", chart_tag, gjson.chart_data[j].image_ready));
					//console.log(sprintf("__get_image_nm3= %s, chrt= %s xb= %.3f img_rdy= %s j= %d", image_nm, use_chart_tag, xb, gjson.chart_data[j].image_ready, j));
					if (gjson.chart_data[j].image_ready === true) {
						ctx.drawImage(gjson.chart_data[j].image, xb, yb, xe-xb, ye-yb);
						if (whch_txt >= 0) {
							gjson.chart_data[j].image_whch_txt = whch_txt;
						} else if (subtst >= 0) {
							gjson.chart_data[j].image_whch_txt = subtst;
						} else {
							gjson.chart_data[j].image_whch_txt = subtst;
						}
						//console.log(sprintf("by_phase: set cd[%d].whch_txt= %d", j, whch_txt));
						img_idx = j;
					}
				}
				break;
			}
			return img_idx;
		}

		function get_num(is_x, x, ymax, ydivi) {
			if (typeof get_num.arr === 'undefined') {
				get_num.obj = {};
				get_num.arr = [];
			}
			let rel = false;
			let ychg = 0.0;
			if (ydivi > 0.0 && ymax > 0.0) {
				ychg = ymax/ydivi;
			}
			if (typeof x === 'string') {
				let x_orig = x;
				if (x.indexOf("_ydivi_") > 0) {
					x = x.replace(/_ydivi_/g, ychg);
				}
				if (x.indexOf("_ymax_") > 0) {
					x = x.replace(/_ymax_/g, ymax);
				}
				if (x.indexOf("%") > 0) {
					rel = false;
					if (is_x) {
						x = x.replace(/%/g, "*0.01*"+mycanvas.width);
					} else {
						//x = x.replace("%");
						x = x.replace(/%/g, "*0.01*"+ymax);
					}
					/*
					x = 0.01 * parseFloat(x);
					if (is_x) {
						x *= mycanvas.width;
					} else {
						x *= ymax;
					}
					*/
				} else {
					//x = parseFloat(x);
				}
				let xx = x;
				let eq_idx = x.indexOf("=");
				let sv_res = false;
				let nm_idx = -1;
				let nm = null;
				if (eq_idx > 0) {
					nm = x.substr(0, eq_idx).trim();
					xx = x.substr(eq_idx+1);
					sv_res = true;
					if (!(nm in get_num.obj)) {
						get_num.arr.push({nm:nm, val:0.0});
						get_num.obj[nm] = get_num.arr.length - 1;
						//console.log(sprintf("BE_sv0: bef= %s, lhs= %s, rhs= %s idx= %s", x_orig, nm, xx, get_num.obj[nm]));
					}
					//console.log(sprintf("BE_sv: bef= %s, lhs= %s, rhs= %s idx= %s", x_orig, nm, xx, get_num.obj[nm]));
				}
				for (let i=0; i < get_num.arr.length; i++) {
					let nmi = get_num.arr[i].nm;
					let j = xx.indexOf(nmi);
					while(j >=0 ) {
						xx = xx.replace(nmi, get_num.arr[i].val);
						j = xx.indexOf(nmi);
					}
				}
				x = g_BigEval.exec(xx);
				if (sv_res) {
					let i = get_num.obj[nm];
					get_num.arr[i].val = x;
					//console.log(sprintf("BE_sv: bef= %s, lhs= %s, rhs= %s idx= %s, x= %.3f", x_orig, nm, xx, get_num.obj[nm], x));
				}
				/*
				if (nm == "sysc_yend") {
					console.log(sprintf("BE: xo= %s, xx= %s, x= %s", x_orig, xx, x));
				}
				*/
			}
			return [x, rel];
		}

		let text_align = 'left';
		let text_anchor = 'bottom';
		let nfont_sz = 11;
		if (typeof g_cpu_diagram_flds.text_defaults.textAlign !== 'undefined') {
			text_align = g_cpu_diagram_flds.text_defaults.textAlign;
		}
		if (typeof g_cpu_diagram_flds.text_defaults.textBaseline !== 'undefined') {
			text_anchor = g_cpu_diagram_flds.text_defaults.textBaseline;
		}
		if (typeof g_cpu_diagram_flds.text_defaults.font_size !== 'undefined') {
			nfont_sz = g_cpu_diagram_flds.text_defaults.font_size;
		}
		for (let i=0; i < g_cpu_diagram_flds.figures.length; i++) {
			if (typeof g_cpu_diagram_flds.figures[i].figure === 'undefined' ||
				typeof g_cpu_diagram_flds.figures[i].figure.cmds === 'undefined' ||
				g_cpu_diagram_flds.figures[i].figure.cmds.length == 0) {
				continue;
			}
			for (let j=0; j < g_cpu_diagram_flds.figures[i].figure.cmds.length; j++) {
				if (typeof g_cpu_diagram_flds.figures[i].figure.cmds[j].cmd === 'undefined') {
					continue;
				}
				sector = 1; // the default
				if (typeof g_cpu_diagram_flds.figures[i].figure.sector !== 'undefined') {
					sector = g_cpu_diagram_flds.figures[i].figure.sector;
				}
				let ymax = y_shift, ydivi=null, ybrdr=null;
				if (sector == 1) {
					ymax = svg_ymax;
				}
				if (typeof g_cpu_diagram_flds.figures[i].ymax !== 'undefined') {
					ymax = g_cpu_diagram_flds.figures[i].ymax;
				}
				if (typeof g_cpu_diagram_flds.figures[i].ydivi !== 'undefined') {
					ydivi = g_cpu_diagram_flds.figures[i].ydivi;
				}
				if (typeof g_cpu_diagram_flds.figures[i].ybrdr !== 'undefined') {
					ybrdr = g_cpu_diagram_flds.figures[i].ybrdr;
				}
				let scl = xlate(ctx,    1.0,    1.0, 0, svg_xmax, 0, svg_ymax, null, sector);
				if (typeof g_cpu_diagram_flds.figures[i].figure.cmds[j].use !== 'undefined' &&
					   g_cpu_diagram_flds.figures[i].figure.cmds[j].use == 'n') {
					   continue;
				}
				if (g_cpu_diagram_flds.figures[i].figure.cmds[j].cmd == 'rect') {
					let wide_relative = [false, false];
					let high_relative = [false, false];

					let x= g_cpu_diagram_flds.figures[i].figure.cmds[j].x;
					let y= g_cpu_diagram_flds.figures[i].figure.cmds[j].y;
					let x_orig = x;
					let y_orig = y;
					let xend_orig=null, yend_orig=null;
					let xe_abs = false, wd;
					let ye_abs = false, hi;
					if (typeof g_cpu_diagram_flds.figures[i].figure.cmds[j].wide !== 'undefined') {
						wd= g_cpu_diagram_flds.figures[i].figure.cmds[j].wide;
					} else {
						wd= g_cpu_diagram_flds.figures[i].figure.cmds[j].xend;
						xe_abs = true;
						xend_orig = wd;
					}
					if (typeof g_cpu_diagram_flds.figures[i].figure.cmds[j].high !== 'undefined') {
						hi= g_cpu_diagram_flds.figures[i].figure.cmds[j].high;
					} else {
						hi= g_cpu_diagram_flds.figures[i].figure.cmds[j].yend;
						ye_abs = true;
						yend_orig = hi;
					}
					last_box_top   = y;
					last_box_bot   = hi;
					last_box_left  = x;
					last_box_right = wd;
					[x,  wide_relative[0]] = get_num(true,   x, ymax, ydivi);
					[y,  high_relative[0]] = get_num(false,  y, ymax, ydivi);
					[wd, wide_relative[1]] = get_num(true,  wd, ymax, ydivi);
					[hi, high_relative[1]] = get_num(false, hi, ymax, ydivi);
					if (xe_abs) {
						wd = wd - x;
					}
					if (ye_abs) {
						hi = hi - y;
					}
					let p0, p1;
					p0 = xlate(ctx,    x,    y, 0, svg_xmax, 0, svg_ymax, null, sector);
					p1 = xlate(ctx, x+wd, y+hi, 0, svg_xmax, 0, svg_ymax, null, sector);
					if (!wide_relative[0]) {
						p0[0] /= scl[0];
					}
					if (!wide_relative[1]) {
						p1[0] /= scl[0];
					}
					if (!high_relative[0]) {
						p0[1] /= scl[1];
					}
					if (!high_relative[1]) {
						p1[1] /= scl[1];
					}
					//console.log(sprintf("fig[%d][%d] rect bef: x= %.2f, y= %.2f, wd= %.2f, hi= %.2f", i, j, x, y, x+wd, y+hi));
					//console.log(sprintf("fig[%d][%d] rect aft: x= %.2f, y= %.2f, wd= %.2f, hi= %.2f", i, j, p0[0], p0[1], p1[0], p1[1]));
					let fill_clr = 'white';
					let strk_clr = 'black';
					if (typeof g_cpu_diagram_flds.figures[i].figure.cmds[j].fillColor !== 'undefined') {
						fill_clr = g_cpu_diagram_flds.figures[i].figure.cmds[j].fillColor;
						//console.log("fillColor: "+fill_clr);
					}
					if (typeof g_cpu_diagram_flds.figures[i].figure.cmds[j].strokeStyle !== 'undefined') {
						strk_clr = g_cpu_diagram_flds.figures[i].figure.cmds[j].strokeStyle;
						//console.log("strokeStyle: "+strk_clr);
					}
					if (fill_clr !== null && fill_clr != 'none') {
						ctx.fillStyle = fill_clr;
						ctx.fillRect(p0[0],p0[1], p1[0]-p0[0], p1[1]-p0[1]);
						last_box_left_px  = p0[0];
						last_box_right_px = p1[0];
						last_box_top_px   = p0[1];
						last_box_bot_px   = p1[1];
						/*
						if (yend_orig.indexOf("sysc_yend") >= 0) {
						console.log(sprintf("fig[%d][%d] rect aft: x= %.2f, y= %.2f, wd= %.2f, hi= %.2f, ye= %s,  y= %.3f, hi= %.3f, ye_abs= %s",
									i, j, p0[0], p0[1], p1[0], p1[1], yend_orig, y, hi, ye_abs));
						}
						*/
					}
					if (strk_clr !== null && strk_clr != 'none') {
						ctx.strokeStyle = strk_clr;
						ctx.strokeRect(p0[0],p0[1], p1[0]-p0[0], p1[1]-p0[1]);
					}
				}
				if (g_cpu_diagram_flds.figures[i].figure.cmds[j].cmd == 'text') {
					ctx.save();
					let wide_relative = [false, false];
					let high_relative = [false, false];
					let box_data = null, box_data_arr = null, lkfor_box = {nm:null, fig_idx:-1, cmd_idx:-1, box_idx:-1};
					if (typeof g_cpu_diagram_flds.figures[i].figure.cmds[j].box_data_arr !== 'undefined') {
					  loop_ka:
					  for (let ka=0; ka < g_cpu_diagram_flds.figures[i].figure.cmds[j].box_data_arr.length; ka++) {
						lkfor_box.nm = g_cpu_diagram_flds.figures[i].figure.cmds[j].box_data_arr[ka].box_nm;
						//console.log(sprintf("__box data_arr[%d].nm= %s", ka, lkfor_box.nm));
						lkfor_box.fig_idx = i;
						lkfor_box.cmd_idx = j;
						lkfor_box.box_idx = ka;
						//box_data     = g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr[kb].box_data;
						box_data_arr = g_cpu_diagram_flds.figures[i].figure.cmds[j].box_data_arr[ka];
						//console.log(sprintf("__box start cmd.txt fig[%d].cmd[%d].box[%d].txt: nm= %s", i, j, ka, lkfor_box.nm));
						add_box_txt(box_data_arr, lkfor_box);
						//break;
						/*
						loop_kf:
						for (let kf=0; kf < g_cpu_diagram_flds.figures.length; kf++) {
							if (typeof g_cpu_diagram_flds.figures[kf].figure.cmds === 'undefined') {
								continue;
							}
							loop_kc:
							for (let kc=0; kc < g_cpu_diagram_flds.figures[kf].figure.cmds.length; kc++) {
								if (typeof g_cpu_diagram_flds.figures[kf].figure.cmds[kc].cmd === 'undefined' ||
									g_cpu_diagram_flds.figures[kf].figure.cmds[kc].cmd !== 'text' ||
									typeof g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr === 'undefined') {
									continue;
								}
								loop_kb:
								for (let kb=0; kb < g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr.length; kb++) {
									if (g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr[kb].box_nm !== lkfor_box.nm ||
										typeof g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr[kb].box_data !== 'undefined') {
										lkfor_box.fig_idx = kf;
										lkfor_box.cmd_idx = kc;
										lkfor_box.box_idx = kb;
										console.log(sprintf("in fig[%d].cmd[%d].box[%d].txt: got box_data nm= %s", kf, kc, kb, lkfor_box.nm));
										//box_data     = g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr[kb].box_data;
										box_data_arr = g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr[kb];
										break loop_ka;
									}
								}
								
							}
						}
						*/
						//console.log("__box_prt txt ck: bda, bda.bd: ", lkfor_box.nm, box_data_arr === null, box_data_arr.box_data === null);
					  }
					}
					function add_box_txt(box_data_arr, lkfor_box)
					{
						if (typeof box_data_arr === 'undefined' || typeof box_data_arr.box_data === 'undefined' ||
							box_data_arr === null || box_data_arr.box_data === null) {
							return;
						}
						//console.log(sprintf("__box_prt txt.str: for box= %s", lkfor_box.nm));
						// save off the box_data_arr.str string
						let str = box_data_arr.str;
						let arr = box_data_arr.box_data.arr.sort(function(a, b){return b.val - a.val});
						//console.log("__box_data_arr: ", box_data_arr);
						//console.log("__box_data_arr arr: ", arr);
						let tstr="", tval="";
						let tlimit = 3;
						if (typeof box_data_arr.limit !== 'undefined') {
							tlimit = box_data_arr.limit;
						}
						for (let ki=0; ki < arr.length; ki++) {
							if (ki > 0) {
								tstr += "; ";
							}
							tstr += arr[ki].nm + ":" + arr[ki].str;
							if ((ki+1) >= tlimit) {
								break;
							}
						}
						box_data_arr.str = str + tstr;
						if (lkfor_box.nm == 'syscall_active') {
							for (let ki=0; ki < arr.length; ki++) {
								if (arr[ki].nm == "pselect6") {
									//console.log(sprintf("__box_prt box_nm= %s, area= %s, val= %s", lkfor_box.nm, arr[ki].nm, arr[ki].str));
									break;
								}
							}
						}
						let y_xtra=0;
						let xb, xe, yb, ye;
						[xb, xe, yb, ye] = draw_fig_text(box_data_arr, y_xtra, false);
						//console.log(sprintf("__box_prt txt.str: for box= %s, str= %s, x(%.0f-%.0f), y(%.0f-%.0f)", lkfor_box.nm, box_data_arr.str, xb, xe, yb, ye));
						box_data_arr.str = str;
						if (lkfor_box.nm == 'syscall_outstanding') {
							let sys_sleep = ["ppoll", "nanosleep", "futex", "pselect", "wait", "epoll"];
							tlimit = 4;
							tstr = "";
							let skip_it=false, did_strs = 0;
							for (let ki=0; ki < arr.length; ki++) {
								if (did_strs > 0) {
									tstr += "; ";
								}
								skip_it = false;
								for (let kk=0; kk < sys_sleep.length; kk++) {
									if (arr[ki].nm.indexOf(sys_sleep[kk]) >= 0) {
										skip_it = true;
										break;
									}
								}
								if (skip_it) {
									continue;
								}
								tstr += arr[ki].nm + ":" + arr[ki].str;
								did_strs += 1;
								if (did_strs >= tlimit) {
									break;
								}
							}
							y_xtra = nfont_sz;
							box_data_arr.str = "top outst non-sleep: "+ tstr;
							draw_fig_text(box_data_arr, y_xtra, false);
							box_data_arr.str = str;
						}
					}

					let cmd_obj = g_cpu_diagram_flds.figures[i].figure.cmds[j];
					let str = cmd_obj.str;
					if (typeof cmd_obj.image_nm !== 'undefined') {
						//console.log("__image_nm= "+cmd_obj.image_nm);
						ctx.fillStyle = 'white';
						ctx.fillRect(last_box_left_px, last_box_top_px, last_box_right_px-last_box_left_px,
								last_box_bot_px-last_box_top_px);
						let img_idx = get_image(ctx, whch_txt, i, j, false, null, cmd_obj.image_nm, last_box_left_px, last_box_right_px, last_box_top_px, last_box_bot_px);
						if (typeof g_cpu_diagram_flds.figures[i].figure.cmds[j].tbox_img === 'undefined') {
							g_cpu_diagram_flds.figures[i].figure.cmds[j].tbox_img = {};
						}
						//g_cpu_diagram_flds.figures[i].figure.cmds[j].tbox_img[chart_data.chart_tag] = {img_idx:img_idx, xb:last_box_left_px, yb:last_box_top_px, xe:last_box_right_px, ye:last_box_bot_px};
						g_cpu_diagram_flds.figures[i].figure.cmds[j].tbox_img = {img_idx:img_idx, xb:last_box_left_px, yb:last_box_top_px, xe:last_box_right_px, ye:last_box_bot_px};
						//console.log("__image_nm_upd= "+cmd_obj.image_nm);
					} else if (str == "Applications") {
						let img_idx = get_image(ctx, whch_txt, i, j, true, "PCT_BUSY_BY_CPU", null, last_box_left_px, last_box_right_px, last_box_top_px, last_box_bot_px);
						//console.log("got img_idx= "+img_idx);
						g_cpu_diagram_flds.figures[i].figure.cmds[j].tbox_fl = {img_idx:img_idx, xb:last_box_left_px, yb:last_box_top_px, xe:last_box_right_px, ye:last_box_bot_px};
					}
					function draw_fig_text(co, y_xtra, do_xlate)
					{
						let x   = co.x;
						let y   = co.y;
						let str = co.str;
						if (typeof y === 'string' && y == "_box_top_") {
							y = last_box_top;
						}
						if (typeof y === 'string' && y == "_box_bot_") {
							y = last_box_bot;
						}
						let ckit=false;
						if (typeof x === 'string' && x == "_box_left_") {
							x = last_box_left;
						}
						if (typeof y === 'string' && y == "_box_ymid_") {
							let ytop, ybot;
							[ytop,  high_relative[0]] = get_num(false,  last_box_top, ymax, ydivi);
							[ybot,  high_relative[0]] = get_num(false,  last_box_bot, ymax, ydivi);
							y = ytop + 0.5 * (ybot - ytop);
							//console.log(sprintf("__ymid= tp= %s, bt= %s, y= %.3f", ytop, ybot, y));
							ckit=true;
						}
						if (typeof x === 'string' && x == "_box_xmid_") {
							let xleft, xrght;
							[xleft, wide_relative[0]] = get_num(true,   last_box_left, ymax, ydivi);
							[xrght, wide_relative[0]] = get_num(true,   last_box_right, ymax, ydivi);
							x = xleft + 0.5 * (xrght - xleft);
							//console.log(sprintf("__xmid= left= %s, rght= %s, x= %.3f", xleft, xrght, x));
						}
						[x,  wide_relative[0]] = get_num(true,   x, ymax, ydivi);
						[y,  high_relative[0]] = get_num(false,  y, ymax, ydivi);
						let text_align2 = text_align, text_anchor2 = text_anchor, nfont_sz2 = nfont_sz;
						if (typeof co.textAlign !== 'undefined') {
							text_align2 = co.textAlign;
						}
						if (typeof co.textBaseline !== 'undefined') {
							text_anchor2 = co.textBaseline;
						}
						if (typeof co.font_size !== 'undefined') {
							nfont_sz2 = co.font_size;
						}
						ctx.textAlign = text_align2;
						ctx.textBaseline = text_anchor2;
						ctx.font = sprintf("%.3fpx sans-serif", nfont_sz2);
						let p0;
						y += y_xtra;
						if (do_xlate) {
							p0 = xlate(ctx,    x,    y, 0, svg_xmax, 0, svg_ymax, null, sector);
							if (!wide_relative[0]) {
								p0[0] /= scl[0];
							}
							if (!high_relative[0]) {
								p0[1] /= scl[1];
							}
						} else {
							p0 = [x, y];
						}
						/*
						if (ckit) {
							console.log(sprintf("__ymid= x= %s, y= %s, p0= %.3f, p1= %.3f", x, y, p0[0], p0[1]));
						}
						*/
						let fill_clr = 'white';
						let strk_clr = 'black';
						ctx.fillStyle = strk_clr;
						ctx.fillText(str, p0[0], p0[1]);
						return get_text_box(p0, str, text_align2, text_anchor2, nfont_sz2);
					}
					let xb, xe, yb, ye;
					[xb, xe, yb, ye] = draw_fig_text(cmd_obj, 0.0, true);
					g_cpu_diagram_flds.figures[i].figure.cmds[j].tbox = {xb:xb, yb:yb, xe:xe, ye:ye};
					ctx.restore();
				}
			}
		}
		ctx.restore();
	}

	function build_pie_data(grf_def_idx, whch_txt, txt_tbl) {
		let pdata = [];
		let grf_name  = g_cpu_diagram_flds.cpu_diagram_fields[grf_def_idx].grf_def.nm;
		for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].grf === 'undefined' ||
				typeof g_cpu_diagram_flds.cpu_diagram_fields[j].grf.nm === 'undefined' ||
				g_cpu_diagram_flds.cpu_diagram_fields[j].grf.nm != grf_name) {
					continue;
			}
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr === 'undefined') {
				continue;
			}
			let typ_data = g_cpu_diagram_flds.cpu_diagram_fields[j].grf.typ_data;
			let mx = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length;
			let pfx = null;
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].pfx !== 'undefined') {
				pfx = g_cpu_diagram_flds.cpu_diagram_fields[j].pfx;
			}
			let fmt_str = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_fmt;
			if (typeof typ_data !== 'undefined') {
				//console.log("got pie data typ= "+typ_data);
				let fllw_arr = g_cpu_diagram_flds.cpu_diagram_fields[j].follow_arr;
				//console.log("follow_arr: ",fllw_arr);
				if (typ_data != 'internal') {
					return pdata;
				}
				for (let ii=0; ii < g_cpu_diagram_flds.cpu_diagram_fields[j].follow_arr.length; ii++) {
					if (pdata.length <= ii) {
						pdata.push([]);
					}
					let ct = g_cpu_diagram_flds.cpu_diagram_fields[j].chart_tag;
					pdata[ii].push({label: "%idle", value: fllw_arr[ii].idle, evt_str:"core "+ii, chart_tag:ct, "slc_lbl":"%idle"});
					if (fllw_arr[ii].follow_proc === null) {
						pdata[ii].push({label: "%busy", value: fllw_arr[ii].other, evt_str:"core "+ii, chart_tag:ct, "slc_lbl":"%busy"});
					} else {
						pdata[ii].push({label: "%"+fllw_arr[ii].follow_proc, value: fllw_arr[ii].follow, evt_str:"core "+ii, chart_tag:ct, "slc_lbl":"%"+fllw_arr[ii].follow_proc});
						pdata[ii].push({label: "%busy_other", value: fllw_arr[ii].other, evt_str:"core "+ii, chart_tag:ct, "slc_lbl":"%busy_other"});
					}
				}
				return pdata;
			}
			//console.log("grf["+j+"] nm mtch");
			for (let ii=0; ii < g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length; ii++) {
				if (pdata.length <= ii) {
					pdata.push([]);
				}
				let val;
				let ti=-1;
				for (ti=0; ti < txt_tbl.length; ti++) {
					if (txt_tbl[ti].jidx == j) {
						break;
					}
				}
				if (ti >= txt_tbl.length) {
					continue;
				}
				let jidx = txt_tbl[ti].jidx;
				let uval = txt_tbl[ti].txt[ii].val;
				let ct = g_cpu_diagram_flds.cpu_diagram_fields[jidx].chart_tag;
				if (pfx === null) {
					pfx = g_cpu_diagram_flds.cpu_diagram_fields[jidx].y_label;
				}
				//console.log("txt_tbl["+ti+"]:", txt_tbl[ti]);
				let evt_str = g_cpu_diagram_flds.cpu_diagram_fields[jidx].tot_line.evt_str[ii];
				pdata[ii].push({label: pfx, value: uval, evt_str:evt_str, chart_tag:ct});
			}
		}
		//console.log(sprintf("build_pie_data: grf_name= %s, whch_txt= %d, len= %d", grf_name, whch_txt, pdata.length));
		//console.log("pie_data: ", pdata);
		return pdata;
	}

	draw_svg([], -1, -1);

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
			let arr3 = find_overlap(x, y);
			let arr = [];
			for (let j=0; j < arr3.length; j++) {
				arr.push(arr3[j].j);
			}
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
		if (g_cpu_diagram_flds !== null) {
			if (typeof g_cpu_diagram_flds.figures !== 'undefined') {
				for (let j=0; j < g_cpu_diagram_flds.figures.length; j++) {
					if (typeof g_cpu_diagram_flds.figures[j].figure.cmds === 'undefined') {
						continue;
					}
					for (let k=0; k < g_cpu_diagram_flds.figures[j].figure.cmds.length; k++) {
					if (typeof g_cpu_diagram_flds.figures[j].figure.cmds[k].tbox === 'undefined') {
						continue;
					}
					let tbox, img_shape;
					if (typeof g_cpu_diagram_flds.figures[j].figure.cmds[k].tbox_fl !== 'undefined' &&
						typeof g_cpu_diagram_flds.figures[j].figure.cmds[k].tbox_fl.img_idx !== 'undefined') {
						tbox = g_cpu_diagram_flds.figures[j].figure.cmds[k].tbox_fl;
						if (typeof gjson.chart_data[tbox.img_idx] !== 'undefined' &&
							typeof gjson.chart_data[tbox.img_idx].fl_image_ready !== 'undefined' &&
							gjson.chart_data[tbox.img_idx].fl_image_ready) {
							img_shape =  gjson.chart_data[tbox.img_idx].fl_image_shape;
						} else {
							continue;
						}
					} else if (typeof g_cpu_diagram_flds.figures[j].figure.cmds[k].tbox_img !== 'undefined' &&
						typeof g_cpu_diagram_flds.figures[j].figure.cmds[k].tbox_img.img_idx !== 'undefined') {
						tbox = g_cpu_diagram_flds.figures[j].figure.cmds[k].tbox_img;
						if (typeof tbox.img_idx !== 'undefined' && typeof gjson.chart_data[tbox.img_idx] !== 'undefined') {
							img_shape =  gjson.chart_data[tbox.img_idx].image_data.shape;
						} else {
							continue;
						}
					} else {
						continue;
					}
					/*
					console.log(sprintf("ck tbox fig[%d].cmds[%d] x= %.2f, y= %.2f, xb= %.2f, xe= %.2f, yb= %.2f, ye= %.2f"+
								"\nshape_orig: hi= %.2f, wd= %.2f, scaled_img: hi= %.2f, wd: %.2f",
								j, k, x, y, tbox.xb, tbox.xe, tbox.yb, tbox.ye,
								img_shape.high, img_shape.wide, tbox.ye-tbox.yb, tbox.xe-tbox.wb));
					*/
					if (((tbox.xb <= x && x <= tbox.xe) ||
						 (tbox.xe <= x && x <= tbox.xb)) &&
						((tbox.yb <= y && y <= tbox.ye) ||
						 (tbox.ye <= y && y <= tbox.yb))) {
						//console.log("tbox.txt1= "+ tbox.txt);
						x_0 = x;
						y_0 = y;
						got_tbox = true;
						arr.push({j:(-j-1), fig:j, cmds:k, img_idx:tbox.img_idx, fl_grf:true, m:null}); 
					}
					}
				}
			}
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
					arr.push({j:(-j-1), m:null}); 
				}
			}
			for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
				if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].bars !== 'undefined') {
					let bars = g_cpu_diagram_flds.cpu_diagram_fields[j].bars;
					//console.log("bars: ", bars);
					for (let m=0; m < bars.length; m++) {
						let xb = bars[m].arr[0].bar_shape.x0;
						let xe = bars[m].arr[0].bar_shape.x0+bars[m].arr[0].bar_shape.wide;
						let yb = bars[m].arr[0].bar_shape.y0;
						let ye = bars[m].arr[0].bar_shape.y0+bars[m].arr[0].bar_shape.high;
						if (((xb <= x && x <= xe) || (xe <= x && x <= xb)) &&
							((yb <= y && y <= ye) || (ye <= y && y <= yb))) {
							//console.log("bars[m].txt1= "+ bars[m].arr[0].evt_str);
							x_0 = x;
							y_0 = y;
							//got_bars = true;
							arr.push({j:(-j-1), m:m}); 
							break;
						}
					}
				}
				if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].pies === 'undefined') {
					continue;
				}
				let pies = g_cpu_diagram_flds.cpu_diagram_fields[j].pies;
				//console.log(sprintf("pies.len= %d", pies.length));
				for (let m=0; m < pies.length; m++) {
					//console.log("pies["+m+"]:", pies[m]);
					let xb = pies[m].x - pies[m].wide/2;
					let xe = pies[m].x + pies[m].wide/2;
					let yb = pies[m].y - pies[m].high/2;
					let ye = pies[m].y + pies[m].high/2;
					if (((xb <= x && x <= xe) ||
						 (xe <= x && x <= xb)) &&
						((yb <= y && y <= ye) ||
						 (ye <= y && y <= yb))) {
							let xFromCenter = x - pies[m].x;
							let yFromCenter = -y + pies[m].y; // because y0 is at top
							let distanceFromCenter = Math.sqrt( Math.pow( Math.abs( xFromCenter ), 2 ) + Math.pow( Math.abs( yFromCenter ), 2 ) );
								console.log(sprintf("try match pies[%d], dist= %.3f, radius= %.3f",
											m, distanceFromCenter, pies[m].wide/2));
							if (distanceFromCenter <= pies[m].wide/2) {
								let clickAngle = Math.atan2( yFromCenter, xFromCenter );
								/* atan2 returns upper half of circle going from 0 -> pi in counter clockwise dir
								 * starting at 0 at 3 o'clock
								 * and bottom half of circle goes from 0 to -PI.
								 * The angles for the pie slices are stored from PI/2 (at 12 o'clock) and
								 * increasing to 5*PI/2 (at 12 o'clock).
								 * So we have to translate the atan2 angle to match the range in the pie array.
								 */
								console.log(sprintf("clickAngle= %.3f", clickAngle));
								if (clickAngle <= (0.5*Math.PI) && clickAngle >= 0.0) {
									clickAngle = 0.5*Math.PI + (0.5*Math.PI - clickAngle);
									console.log(sprintf("clickAngl0= %.3f", clickAngle));
								}
								else if (clickAngle >= (-0.5*Math.PI) && clickAngle <= 0.0) {
									clickAngle = 0.5*Math.PI + (0.5*Math.PI - clickAngle);
									console.log(sprintf("clickAngl1= %.3f", clickAngle));
								}
								else if (clickAngle >= (-Math.PI) && clickAngle <= (0.5*Math.PI)) {
									clickAngle = 0.5*Math.PI + (0.5*Math.PI - clickAngle);
									console.log(sprintf("clickAngl2= %.3f", clickAngle));
								}
								else if (clickAngle >= (0.5*Math.PI) && clickAngle <= (Math.PI)) {
									clickAngle = 2.0*Math.PI + (Math.PI - clickAngle);
									console.log(sprintf("clickAngl3= %.3f", clickAngle));
								}
								console.log(sprintf("rad= %.3f, deg= %.3f", clickAngle, 360.0* clickAngle/(2.0*Math.PI)));
								let got_slc=-1;
								console.log(sprintf("match[%d]: arr.len= %d", m, pies[m].arr.length));
								console.log("match: ", pies[m]);
								for (let slc=0; slc < pies[m].arr.length; slc++) {
									console.log(sprintf("try match[%d] ab= %.3f a= %.3f ae= %.3f val= %.3f",
												slc, pies[m].arr[slc].begAngle, clickAngle, pies[m].arr[slc].endAngle, pies[m].arr[slc].value));
									if ((pies[m].arr[slc].begAngle <= clickAngle && clickAngle <= pies[m].arr[slc].endAngle) ||
										(pies[m].arr[slc].begAngle >= clickAngle && clickAngle >= pies[m].arr[slc].endAngle)) {
										console.log(sprintf("match[%d] val= %.3f", slc, pies[m].arr[slc].value));
										got_slc=slc;
										break;
									}
								}

								console.log(sprintf("match pies[%d]", m));
								got_tbox = true;
								arr.push({j:(-j-1), m:m, slc:got_slc}); 
							}
					}
				}
			}
		}
		for (let i=0; i < shapes.length; i++) {
			if (shapes[i].box !== null) {
				if (((shapes[i].box.xb <= x && x <= shapes[i].box.xe) ||
					 (shapes[i].box.xe <= x && x <= shapes[i].box.xb)) &&
					((shapes[i].box.yb <= y && y <= shapes[i].box.ye) ||
					 (shapes[i].box.ye <= y && y <= shapes[i].box.yb))) {
					arr.push({j:i, m:null});
				}
			}
			if (typeof shapes[i].poly !== 'undefined') {
				if (isPointInPoly(shapes[i].poly, {x, y})) {
					arr.push({j:i, m:null});
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
				typeof shapes[i].is_connector !== 'undefined' &&
				shapes[i].is_connector == true ) {
				//console.log(shapes[i]);
				let ply_str = "", cma="";
				let arr = [];
				for (let j=0; j < shapes[i].poly.length; j++) {
					let con_str = "", con_cma= "";
					if (typeof shapes[i].poly[j].im_connected_to !== 'undefined') {
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
				if (typeof shapes[i].is_connected_to_me !== 'undefined') {
					for (let m=0; m < shapes[i].is_connected_to_me.length; m++) {
						let cf = shapes[i].is_connected_to_me[m];
						if (
							shapes[cf].typ == 'path' &&
							typeof shapes[cf].is_connector !== 'undefined' &&
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

	function get_text_box(p0, str, text_align, text_anchor, nfont_sz) {
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
			ye = yb + nfont_sz;
		} else if (text_anchor == 'bottom' || text_anchor == 'alphabetic') {
			ye = p0[1];
			yb = ye - nfont_sz;
		} else if (text_anchor == 'middle' || text_anchor == 'center') {
			yb = p0[1] - 0.5*nfont_sz;
			ye = p0[1] + 0.5*nfont_sz;
		} else {
			console.log("unknown text_anchor= "+text_anchor);
			ye = p0[1];
			yb = ye - nfont_sz;
		}
		return [xb, xe, yb, ye]
	}

	function draw_svg_txt_flds(whch_txt, subtst)
	{
		if (g_cpu_diagram_flds === null) {
			return [];
		}
		let fmxx = g_cpu_diagram_flds.cpu_diagram_hdr.max_x;
		let fmxy = g_cpu_diagram_flds.cpu_diagram_hdr.max_y;
		let ftr_str = draw_svg_footer(fmxx, fmxy, g_cpu_diagram_flds.cpu_diagram_copyright);
		if (whch_txt < -1) {
			return [];
		}
		let txt_tbl = [];
		for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
			if ( typeof g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr === 'undefined') {
				continue;
			}
			if (whch_txt == -1) {
				let len = g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line.xarray.length;
				g_cpu_diagram_flds.xbeg = g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line.xarray[0];
				g_cpu_diagram_flds.xend = g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line.xarray[len-1];
			} else {
				g_cpu_diagram_flds.xbeg = g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line.xarray[whch_txt];
				g_cpu_diagram_flds.xend = g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line.xarray[whch_txt+1];
			}
			let ci     = g_cpu_diagram_flds.cpu_diagram_fields[j].chrt_idx;
			let cd     = gcanvas_args[ci][2]; // chart_data
			let t0     = cd.ts_initial.ts;
			g_cpu_diagram_flds.xbeg += t0;
			g_cpu_diagram_flds.xend += t0;
			break;
		}
		let jtxt = g_cpu_diagram_canvas.json_text;
		if (typeof g_cpu_diagram_canvas.json_text === 'undefined' || g_cpu_diagram_canvas.json_text === null) {
			g_cpu_diagram_canvas.json_text = '{"txt":[';
		}
		console.log(sprintf("__svg_txt wcht_txt= %d, subtst= %d", whch_txt, subtst));
		if (whch_txt == 0 || subtst == 0) {
			g_cpu_diagram_canvas.json_text += "{";
		} else {
			g_cpu_diagram_canvas.json_text += ", {";
		}
		let whch_txt_idx = whch_txt;
		if (subtst != -1) {
			whch_txt_idx = subtst;
		}
		g_cpu_diagram_canvas.json_text += '"lp":'+whch_txt_idx;
		let hdr_obj = draw_svg_header(whch_txt_idx, g_cpu_diagram_flds.xbeg, g_cpu_diagram_flds.xend, true, g_svg_scale_ratio, false);
		g_cpu_diagram_canvas.json_text += ', "key_val_arr":[';
		let text_align = 'left';
		let text_anchor = 'bottom';
		//let text_anchor = 'top';
		let nfont_sz = 11;
		if (typeof g_cpu_diagram_flds.text_defaults.textAlign !== 'undefined') {
			text_align = g_cpu_diagram_flds.text_defaults.textAlign;
		}
		if (typeof g_cpu_diagram_flds.text_defaults.textBaseline !== 'undefined') {
			text_anchor = g_cpu_diagram_flds.text_defaults.textBaseline;
		}
		if (typeof g_cpu_diagram_flds.text_defaults.font_size !== 'undefined') {
			nfont_sz = g_cpu_diagram_flds.text_defaults.font_size;
		}
    	ctx.save();
		for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
			if ( typeof g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr === 'undefined') {
				continue;
			}
			let sector = 1;
			// need to get box_nm match to save off data value array for later when we add text to OS diagram
			let use_box_nm = {nm:null, cmd_idx:-1, fig_idx:-1, box_idx:-1};
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].fld.use_box_nm !== 'undefined') {
				use_box_nm.nm = g_cpu_diagram_flds.cpu_diagram_fields[j].fld.use_box_nm;
				use_box_nm.ref_idx = j;
				loop_kf:
				for (let kf=0; kf < g_cpu_diagram_flds.figures.length; kf++) {
					if (typeof g_cpu_diagram_flds.figures[kf].figure.cmds === 'undefined') {
						continue;
					}
					loop_kc:
					for (let kc=0; kc < g_cpu_diagram_flds.figures[kf].figure.cmds.length; kc++) {
						if (typeof g_cpu_diagram_flds.figures[kf].figure.cmds[kc].cmd === 'undefined' ||
							g_cpu_diagram_flds.figures[kf].figure.cmds[kc].cmd !== 'text' ||
							typeof g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr === 'undefined') {
							continue;
						}
						loop_kb:
						for (let kb=0; kb < g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr.length; kb++) {
							if (g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr[kb].box_nm == use_box_nm.nm) {
								use_box_nm.box_data_arr = g_cpu_diagram_flds.figures[kf].figure.cmds[kc].box_data_arr[kb];
								use_box_nm.fig_idx = kf;
								use_box_nm.cmd_idx = kc;
								use_box_nm.box_idx = kb;
								//console.log(sprintf("got box_data_arr into which going to save data vals fig[%d].cmd[%d] box[%d]= %s", kf, kc, kb, use_box_nm.nm));
								break loop_kf;
							}
						}
					}
				}
			}
			for (let jj=0; jj < 2; jj++) {
				let cp_str = "";
				let val_arr = [];
				let ux, uy;
				let vrb = 0;
				if (jj == 0 && typeof g_cpu_diagram_flds.cpu_diagram_fields[j].tfld === 'undefined') {
					continue;
				}
				if (jj == 0) {
					ux = g_cpu_diagram_flds.cpu_diagram_fields[j].tfld.x;
					if (ux < 0) {
						if (typeof g_cpu_diagram_flds.text_defaults.same_x !== 'undefined' &&
							ux == g_cpu_diagram_flds.text_defaults.same_x) {
							ux = g_cpu_diagram_flds.cpu_diagram_fields[j].fld.x;
						}
					}
					uy = g_cpu_diagram_flds.cpu_diagram_fields[j].tfld.y;
					if (uy < 0) {
						if (typeof g_cpu_diagram_flds.text_defaults.row_m1 !== 'undefined' &&
							uy == g_cpu_diagram_flds.text_defaults.row_m1) {
							uy = g_cpu_diagram_flds.cpu_diagram_fields[j].fld.y  - (1.1 * nfont_sz);
						}
					}
					if (uy < 0) {
						if (typeof g_cpu_diagram_flds.text_defaults.row_p1 !== 'undefined' &&
							uy == g_cpu_diagram_flds.text_defaults.row_p1) {
							uy = g_cpu_diagram_flds.cpu_diagram_fields[j].fld.y  + (1.1 * nfont_sz);
						}
					}
					cp_str = g_cpu_diagram_flds.cpu_diagram_fields[j].tfld.text;
				} else {
					let pfx = "";
					let grf = null;
					if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].pfx !== 'undefined') {
						pfx = g_cpu_diagram_flds.cpu_diagram_fields[j].pfx;
					}
					if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].grf !== 'undefined') {
						grf = g_cpu_diagram_flds.cpu_diagram_fields[j].grf;
					}
					if (pfx == "%fe_stalled: l1i_miss:     ") {
						vrb = 1;
					}
					cp_str = pfx;
					ux = g_cpu_diagram_flds.cpu_diagram_fields[j].fld.x;
					uy = g_cpu_diagram_flds.cpu_diagram_fields[j].fld.y;
					let fmt_str = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_fmt;
					let det_arr = [];
					let do_sort = false;
					function Comparator(a, b) {
						if (a.str < b.str) return -1;
						if (a.str > b.str) return 1;
						return 0;
					}
					for (let ii=0; ii < g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length; ii++) {
						let val;
						if (whch_txt == -1) {
							val = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr[ii];
						} else {
							val = g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line.yarray[ii][whch_txt];
						}
						let fmt_str1 = fmt_str;
						let nm_idx = g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line.evt_str_base_val_arr[ii];
						let ci     = g_cpu_diagram_flds.cpu_diagram_fields[j].chrt_idx;
						let cd     = gcanvas_args[ci][2]; // chart_data
						let nm = null;
						if (typeof cd.subcat_rng[nm_idx] !== 'undefined') {
							nm = cd.subcat_rng[nm_idx].cat_text;
						}
						if (fmt_str1.indexOf("__VARNM__") >= 0) {
							fmt_str1 = fmt_str1.replace('__VARNM__', g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line.evt_str[ii]);
							do_sort = true;
						} else if (fmt_str1.indexOf("__BASEVARNM__") >= 0) {
							do_sort = true;
							fmt_str1 = fmt_str1.replace('__BASEVARNM__', nm);
							//fmt_str1 = "ii= "+ii+", nmidx= "+nm_idx+", "+fmt_str1;
						}
						let str = sprintf(fmt_str1, val);
						det_arr.push({str:str, val:val, whch_txt:whch_txt, ii:ii, nm:nm});
						//cp_str += (ii > 0 ? ",":"") + str;
						//g_cpu_diagram_canvas.json_text += ', "'+cp_str+" "+g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line.evt_str[ii]+" "+str+'":'+val;
					}
					if (do_sort) {
						det_arr = det_arr.sort(Comparator);
					}
					for (let ii=0; ii < g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length; ii++) {
						val_arr.push(det_arr[ii]);
						cp_str += (ii > 0 ? ";":"") + val_arr[ii].str;
					}
					if (use_box_nm.cmd_idx != -1) {
						g_cpu_diagram_flds.figures[use_box_nm.fig_idx].figure.cmds[use_box_nm.cmd_idx].box_data_arr[use_box_nm.box_idx].box_data = {arr:val_arr, str:cp_str};
						/*
						console.log(sprintf("got box_data_arr saved data vals fig[%d].cmd[%d] box[%d]= %s",
									use_box_nm.fig_idx, use_box_nm.cmd_idx, use_box_nm.box_idx, use_box_nm.nm));
						*/
					}
				}
				if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].print !== 'undefined' &&
					g_cpu_diagram_flds.cpu_diagram_fields[j].print == 'n') {
					continue;
				}
				let font_sz = sprintf("%.3fpx", nfont_sz);
				//nfont_sz *= g_svg_scale_ratio.svg_xratio;
				let x0 = svg_xmax * ux/fmxx;
				let y0 = svg_ymax * uy/fmxy;
				let p0 = xlate(ctx, x0, y0, 0, svg_xmax, 0, svg_ymax, null, sector);
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
					str = str.replace(/&amp;/g, '&');
				}
				let xb, xe, yb, ye;
				[xb, xe, yb, ye] = get_text_box(p0, str, text_align, text_anchor, nfont_sz);
				//console.log(sprintf("txt %s %s", text_align, text_anchor));
				//console.log(shapes[i].box);
				if (yb > ye) {
					let ybt = ye;
					ye = yb;
					yb = ybt;
				}
				if (false && vrb == 1) {
					console.log(sprintf("__vrb: ancr= %s, x= %.3f, y= %.3f, yb= %.3f, ye= %.3f",
							ctx.textBaseline, p0[0], p0[1], yb, ye));
				}

				ctx.fillText(str, p0[0], p0[1]);
				/*
				ctx.fillText(str, p0[0], yb);
				*/
				if (jj == 0) {
					continue;
				}
				g_cpu_diagram_flds.cpu_diagram_fields[j].tbox = {xb:xb, xe:xe, yb:yb, ye:ye, txt:cp_str};
				let iidx = txt_tbl.length;
				txt_tbl.push({jidx:j, iidx:iidx, txt:[]});
				for (let k=0; k < val_arr.length; k++) {
					txt_tbl[iidx].txt.push(val_arr[k]);
				}
				if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].rng !== 'undefined') {
					let off = 0.0;
					if (ye < yb) {
						ye = yb + nfont_sz;
					}
					let hi = 0.4 * (ye - yb);
					for (let ii=0; ii < g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length; ii++) {
						ctx.beginPath();
						ctx.arc(xe+2+off+hi, yb+hi, hi, 0, 2 * Math.PI, false);
						let cmp_le = true;
						let rng = g_cpu_diagram_flds.cpu_diagram_fields[j].rng;
						if (typeof rng.cmp !== 'undefined' && rng.cmp == 'ge') {
							cmp_le = false;
						}
						//let dv = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr[ii];
						let dv;
						if (whch_txt == -1) {
							dv = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr[ii];
						} else {
							dv = g_cpu_diagram_flds.cpu_diagram_fields[j].tot_line.yarray[ii][whch_txt];
						}
						if (cmp_le) {
							if (dv <= rng.green) {
								ctx.fillStyle = 'green';
							} else if (dv <= rng.yellow) {
								ctx.fillStyle = 'yellow';
							} else {
								ctx.fillStyle = 'red';
							}
						} else {
							if (dv >= rng.green) {
								ctx.fillStyle = 'green';
							} else if (dv >= rng.yellow) {
								ctx.fillStyle = 'yellow';
							} else {
								ctx.fillStyle = 'red';
							}
						}
						off += 2.0*hi;
						ctx.fill();
						ctx.lineWidth = 1;
						ctx.strokeStyle = 'black';
						ctx.stroke();
						ctx.closePath();
					}
				}
			}
		}
    	ctx.restore();
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
			if (typeof txt_tbl[i] !== 'undefined') {
				for (let k=0; k < txt_tbl[i].txt.length; k++) {
					let txt = txt_tbl[i].txt[k].str;
					tbl_str += "<td align='right' title='"+ desc + "'>" + txt + "</td>";
				}
			}
			tbl_str += "</tr>";
		}
		tbl_str += "</table>";
		let tbl_str2 = parse_resource_stalls(txt_tbl, hdr_obj);
		g_cpu_diagram_canvas.json_text += ']} ';
		//mytable.innerHTML = tbl_str2 + tbl_str;
		mytable.innerHTML = tbl_str2;
		return txt_tbl;
	}

	function lkup_chrt(lkup, txt_tbl, chrt, use_pfx) {
		if (typeof lkup[chrt] !== 'undefined') {
			let i = lkup[chrt];
			let j = txt_tbl[i].jidx;
			let pfx = g_cpu_diagram_flds.cpu_diagram_fields[j].y_label;
			if (use_pfx && typeof g_cpu_diagram_flds.cpu_diagram_fields[j].pfx !== 'undefined') {
				pfx = g_cpu_diagram_flds.cpu_diagram_fields[j].pfx;
			}
			let tv = [];
			for (let k=0; k < txt_tbl[i].txt.length; k++) {
				tv.push(txt_tbl[i].txt[k].str);
			}
			return {j:txt_tbl[i].jidx, i:i, desc:g_cpu_diagram_flds.cpu_diagram_fields[j].desc,
				//ylab:g_cpu_diagram_flds.cpu_diagram_fields[j].y_label,
				ylab:pfx,
				vals:g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr,
				rng: g_cpu_diagram_flds.cpu_diagram_fields[j].rng,
				tvals:tv};
		}
		return {j:-1, i:-1};
	}

	function run_OS_view_cputime(lkfor_chart)
	{
		let ch_d = null;
		for (let j=0; j < gjson.chart_data.length; j++) {
			if (lkfor_chart == gjson.chart_data[j].chart_tag) {
				ch_d = gjson.chart_data[j];
				break;
			}
		}
		if (ch_d == null) {
			return [];
		}
		let proc_hsh = {};
		let proc_arr = [];
		let minx = ch_d.last_used_x_min_max.minx;
		let maxx = ch_d.last_used_x_min_max.maxx;
		console.log(sprintf("run_OS: minx= %.3f, maxx= %.3f, shps.len= %d", minx, maxx, ch_d.myshapes.length));
		let cntr = 0;
		//let need_tag = gjson.chart_data[j].file_tag
		for (let i=0; i < ch_d.myshapes.length; i++) {
			let x0 = ch_d.myshapes[i].pts[PTS_X0];
			let x1 = ch_d.myshapes[i].pts[PTS_X1];
			if (x0 == x1) {
				// these are the vertical lines on the cpu_busy chart
				continue;
			}
			if (x0 > maxx) {
				continue;
			}
			if (x1 < minx) {
				continue;
			}
			let operiod = ch_d.myshapes[i].ival[IVAL_PERIOD];
			let fe_idx = ch_d.myshapes[i].ival[IVAL_FE];
            if (typeof fe_idx === 'undefined' || fe_idx < 0) {
				continue;
			}
			let ev = ch_d.flnm_evt[fe_idx].event;
			if (ev != "sched:sched_switch" && ev != "CSwitch") {
				continue;
			}
			let nperiod = operiod;
			let x0t = x0;
			if (x0t < minx) {
				x0t = minx;
			}
			let x1t = x1;
			if (x1t > maxx) {
				x1t = maxx;
			}
			operiod = x1t - x0t;
			let nm = null;
			let cpt= ch_d.myshapes[i].ival[IVAL_CPT];
			if (cpt >= 0 && typeof ch_d.proc_arr[cpt] !== 'undefined') {
				nm = ch_d.proc_arr[cpt].comm;
			}
			if (nm == null) {
				continue;
			}
			if (!(nm in proc_hsh)) {
				proc_hsh[nm] = proc_arr.length;
				proc_arr.push({nm:nm, tot:0.0});
			}
			let idx = proc_hsh[nm];
			proc_arr[idx].tot += operiod;
		}
		console.log(sprintf("run_OS: proc_arr.len= %d, cntr= %d", proc_arr.length, cntr));
		return proc_arr;
	}

	function run_OS_view_tbl(lkup) {
		let t = "";
		let use_top = 4;
		if (typeof g_cpu_diagram_flds.figures_def !== 'undefined' &&
			typeof g_cpu_diagram_flds.figures_def.use_top !== 'undefined') {
			use_top = g_cpu_diagram_flds.figures_def.use_top;
		}
		let prc_arr = run_OS_view_cputime("PCT_BUSY_BY_CPU");
		prc_arr.sort(function(a, b){return b.tot - a.tot;});
		let line_arr = [];
		{
			let t1 = "";
			t1 += "<tr><td>proc cputime</td>";
			for (let k=0; k < prc_arr.length; k++) {
				t1 += "<td>"+prc_arr[k].nm+": "+sprintf("%.3f", prc_arr[k].tot)+"</td>";
				if (k >= use_top) {
					break;
				}
			}
			t1 += "</tr>"
			line_arr.push({fig_idx:-1, cmd_idx:-1, str:t1});
		}
		for (let c=0; c < lkup.length; c++) {
			let ct = lkup[c][0];
			let j  = lkup[c][1];
			let cdf = g_cpu_diagram_flds.cpu_diagram_fields[j];
			let gcdii = cdf.gjson_chart_data_img_idx;
			if (typeof cdf.fig_cmd_idx === 'undefined') {
				continue;
			}
			if (typeof cdf.data_val_arr === 'undefined') {
				console.log(sprintf("__OS_view: %s dva.len= %s", ct, 'undef'));
				continue;
			}
			//console.log(sprintf("__OS_view: %s dva.len= %s", ct, cdf.data_val_arr.length));
			let fig_idx = cdf.fig_cmd_idx.fig;
			let cmd_idx = cdf.fig_cmd_idx.cmd;
			let cmd_obj = g_cpu_diagram_flds.figures[fig_idx].figure.cmds[cmd_idx];
			let v_arr = [];
			let dv_arr = cdf.data_val_arr;
			for (let i=0; i < dv_arr.length; i++) {
				v_arr.push({val:dv_arr[i], idx:i});
			}
			v_arr.sort(function(a, b){return b.val - a.val;});
			let tl = cdf.tot_line;
			if (dv_arr.length != tl.evt_str.length) {
				console.log(sprintf("dv_arr.len= %d, tl.es.len= %d", dv_arr.length, tl.evt_str.length));
				continue;
			}
			let lbl = cmd_obj.str;
			if (typeof lbl === 'undefined') {
				lbl = cdf.y_label;
			}
			let t1 = "";
			t1 += "<tr><td>"+lbl+"</td>";
			for (let k=0; k < dv_arr.length; k++) {
				let kk = v_arr[k].idx;
				t1 += "<td>"+tl.evt_str[kk]+": "+sprintf(cdf.data_val_fmt, v_arr[k].val)+"</td>";
				if (k >= use_top) {
					break;
				}
			}
			t1 += "</tr>"
			line_arr.push({fig_idx:fig_idx, cmd_idx:cmd_idx, str:t1});
		}
		line_arr.sort(function(a, b){
			if (a.fig_idx < b.fig_idx) {
				return -1;
			} else if (a.fig_idx > b.fig_idx) {
				return +1;
			}
			return a.cmd_idx - b.cmd_idx;
		});
		for (let i=0; i < line_arr.length; i++) {
			t += line_arr[i].str;
		}
		return t;
	}

	function lkup_fillin(ret, flds_max, resource, prfx, cma) {
		let stall = "";
		let txt     = "";
		let t = "";
		if (typeof ret.rng === 'undefined') {
			//return t;
		}
		let prefix = "";
		if (typeof prfx !== "undefined") {
			for (let i=0; i < prfx; i++) {
				prefix += "&nbsp;";
			}
		}
		let jtxt = '"vals":[';
		let ys_ar = [];
		let yv_ar = [];
		for (let i=0; i < flds_max; i++) {
			let str = "";
			let cir = ""; 
			if (i < ret.vals.length) {
				str = ret.tvals[i];
				let sml_str = str.trim();
				let spc_idx = sml_str.lastIndexOf(" ");
				if (spc_idx >= 0) {
					let end = sml_str.substr(spc_idx+1);
					yv_ar.push(end);
					let beg = sml_str.substr(0, spc_idx);
					sml_str = end;
					beg = beg.trim();
					if (beg.endsWith(":") || beg.endsWith("=")) {
						beg = beg.substr(0, beg.length-1);
					}
					ys_ar.push(beg);
				}
				let ck_colon = sml_str.indexOf(":");
				let usml_str = sml_str;
				if (ck_colon >= 0) {
					usml_str = usml_str.substr(ck_colon+1);
				}
				jtxt += (i > 0 ? ", " : "") + usml_str;
				if (typeof ret.rng !== 'undefined') {
					let cmp_le = true;
					if (typeof ret.rng.cmp !== 'undefined' && ret.rng.cmp == 'ge') {
						cmp_le = false;
					}
					if (cmp_le) {
						if (ret.vals[i] <= ret.rng.green) {
							cir = "<span class='dot_green'>n</span>";
						} else if (ret.vals[i] <= ret.rng.yellow) {
							cir = "<span class='dot_yellow'>?</span>";
						} else {
							cir = "<span class='dot_red'>y</span>";
						}
					} else {
						if (ret.vals[i] >= ret.rng.green) {
							cir = "<span class='dot_green'>n</span>";
						} else if (ret.vals[i] >= ret.rng.yellow) {
							cir = "<span class='dot_yellow'>?</span>";
						} else {
							cir = "<span class='dot_red'>y</span>";
						}
					}
				}
			}
			stall += "<td title='"+ret.desc+"'>"+cir+"</td>";
			txt += "<td align='right' title='"+ret.desc+"' nowrap='nowrap'>"+str+"</td>";
		}
		if (jtxt.indexOf("%") > 0) {
			jtxt = jtxt.replace(/%/g, "");
		}
		jtxt += ']';
		t += "<tr>";
		t += "<td title='"+ret.desc+"'>" +prefix+ret.ylab + "</td>";
		let jdesc = ret.desc;
		if (jdesc.indexOf("\n") > 0) {
			jdesc = jdesc.replace(/\n/g, "; ");
		}
		let rj = ret.j;
		let tl_estr = g_cpu_diagram_flds.cpu_diagram_fields[rj].tot_line.evt_str;
		let etxt = '"yvar":[';
		if (ys_ar.length > 0) {
			for (let i=0; i < ys_ar.length; i++) {
				let str = '"' + ys_ar[i] + '"';
				etxt += (i > 0 ? ", " : "") + str;
			}
		} else {
			for (let i=0; i < tl_estr.length; i++) {
				let str = '"' + tl_estr[i] + '"';
				etxt += (i > 0 ? ", " : "") + str;
			}
		}
		etxt += ']';
		g_cpu_diagram_canvas.json_text += cma.t + ' {"key":"'+ret.ylab+'", '+jtxt+', '+etxt+', "desc":"'+jdesc+'"}';
		cma.t = ",";
		t += txt;
		if (typeof ret.rng !== 'undefined') {
			t += stall;
		}
		t += "<td title='"+ret.desc+"'>" +resource + "</td>";
		t += "</tr>";
		return t;
	}

	function run_tbl(tbl, txt_tbl, lkup, flds_max, indnt, cma, use_pfx)
	{
		let txt = "";
		for (let i=0; i < tbl.length; i++) {
			let ret = lkup_chrt(lkup, txt_tbl, tbl[i][0], use_pfx);
			if (ret.i > -1) {
				txt += lkup_fillin(ret, flds_max, tbl[i][1], indnt, cma);
			}
		}
		return txt;
	}

	function parse_resource_stalls_arm_cortex_a53(txt_tbl, hdr_obj)
	{
		if (svg_name != "arm_cortex_a53_block_diagram.svg") {
			return "";
		}
		let cpu_typ_str = "ARM Cortex A53";
		let lkup = {};
		let flds_max = 0;
		g_cpu_diagram_flds.lkup_dvals_for_chart_tag = {};
		for (let i=0; i < txt_tbl.length; i++) {
			let j  = txt_tbl[i].jidx;
			let ct = g_cpu_diagram_flds.cpu_diagram_fields[j].chart_tag;
			if (typeof lkup[ct] === 'undefined') {
				g_cpu_diagram_flds.lkup_dvals_for_chart_tag[ct] = [j];
			} else {
				g_cpu_diagram_flds.lkup_dvals_for_chart_tag[ct].push(j);
			}
			lkup[ct] = i;
			let mx = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length;
			let ci = g_cpu_diagram_flds.cpu_diagram_fields[j].chrt_idx;
			let cd = gcanvas_args[ci][2]; // chart_data
			if (typeof cd.tot_line_opts_xform === 'undefined' || cd.tot_line_opts_xform != 'select_vars') {
				if (flds_max < mx) {
					flds_max = mx;
				}
			}
		}
		let cma = {"t":""};

		cma = {"t":""};
		let sys_tbl = [
			["PCT_BUSY_BY_SYSTEM", "Percent busy system"]
			,["DISK_BW_BLOCK_RQ", "Disk bandwidth"]
			,["TEMPERATURE_BY_ZONE", "temperature (degrees C) by zone"]
			,["FREQ_BY_CPU", "Frequency in GHz by CPU"]
			// ,["RAPL_POWER_BY_AREA", "Power (W) by chip area"]  // don't have on arm
			,["BW_BY_TYPE", "Memory bandwidth (MB/sec) by type"] // don't have on arm (yet) but could
			];
		let flds_max_sys = -1;
		for (let k=0; k < sys_tbl.length; k++) {
			let ct = sys_tbl[k][0];
			if (typeof lkup[ct] === 'undefined') {
				continue;
			}
			let i = lkup[ct];
			let j  = txt_tbl[i].jidx;
			//let ct = g_cpu_diagram_flds.cpu_diagram_fields[j].chart_tag;
			let mx = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length;
			if (flds_max_sys < mx) {
				flds_max_sys = mx;
			}
		}
		let t = "";
		t += "<table border='1' style='float: left'>";
		t += "<tr><td>"+cpu_typ_str+" system metrics</td></tr>";
		t += run_tbl(sys_tbl, txt_tbl, lkup, flds_max_sys, 2, cma, true);
		t += "</table>";

		let OS_view_hash = {};
		let OS_view_tbl = [];
		for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].save_image_nm === 'undefined') {
				// if save_image_nm not found then this chart can't be used in the OS_view charts
				continue;
			}
			let ct = g_cpu_diagram_flds.cpu_diagram_fields[j].chart;
			if (ct in OS_view_hash) {
				continue;
			}
			OS_view_hash[ct] = j;
			OS_view_tbl.push([ct, j, "desc: "+ct]);
		}

		t += "<table border='1' style='float: left'>";
		t += "<tr><td>"+cpu_typ_str+" OS view metrics</td></tr>";
		t += run_OS_view_tbl(OS_view_tbl);
		t += "</table>";

		t += "<table border='1' style='float: left'>";
		t += "<tr><td>"+cpu_typ_str+" CPU diagram metrics</td>";
		for (let i=0; i < flds_max; i++) {
			t += "<td title='Value of the computed metric for core or system'>core"+i+" val</td>";
		}
		for (let i=0; i < flds_max; i++) {
			t += "<td title='Some of these are guesses as to whether the core is stalled.'>core"+i+" stalled</td>";
		}
		t += "<td>Resource utilization description</td></tr>";
		t += "<tr><td>Memory Subsystem </td></tr>";

		// arm a53
		let mem_tbl = [
			["BUS_ACCESS_BYTES_PER_CYCLE_CHART", "DRAM: max possible memory BW seems to be about 1.43 bytes/cycle"],
			["BUS_ACCESS_GBYTES_PER_SEC_CHART", "DRAM: max possible memory BW seems to be about 2.0 Gbytes/sec"],
			["L2_MISS_BYTES_PER_CYCLE_CHART", "L2_misses: max L2 misses about 16 bytes/cycle. "],
			["L2_MISS_GBYTES_PER_SEC_CHART", "L2_misses: GB/sec 64e-9*L2_misses/sec. Max is ~2.2 GB/s."],
			["L2D_CACHE_WB_GBYTES_PER_SEC_CHART", "L2_writebacks: GB/sec 64e-9*L2_writebacks/sec. Max is ~2.2 GB/s."],
			["L2_ACCESS_GBYTE_PER_SEC_CHART", "L2_accesses: calculated as 1e-9 * 64 bytes * L2_access/sec."],
			["L2_ACCESS_BYTES_PER_CYCLE_CHART", "L2_accesses: calculated as 64 bytes * L2_access/cycle."],
			["DCACHE_MISS_PER_CYCLE_CHART", "L1d_misses: calculated as  L1d_misses/cycle."],
			["DCACHE_MISS_BYTES_PER_CYCLE_CHART", "L1d_misses: calculated as 64 bytes * L1d_misses/cycle"],
			["L1D_CACHE_WB_BYTES_PER_CYCLE_CHART", "L1d_writebacks: calculated as 64 bytes * L1d_writebacks/cycle"],
			["PREF_BYTES_PER_CYCLE_CHART", "L1d_prefetches: calculated as 64 bytes * L1d_prefetches/cycle"],
			["DCACHE_MISS_RATIO_PCT_CHART", "L1d_miss_ratio: 100 * L1d_misses/L1d_accesses."],
			["DCACHE_TLB_MISS_PER_DCACHE_MISS_CHART", "L1d_tlb_misses: 100*L1d_tlb_miss/L1d_miss"],
		];
		t += run_tbl(mem_tbl, txt_tbl, lkup, flds_max, 2, cma, true);

		t += "<tr><td>Front End</td></tr>";

		let fe_tbl = [
			["ICACHE_MISS_STALLS_CHART", "L1i_miss_stalls: %cycles pipeline stalled due to L1 icache miss"],
			["IUTLB_DEP_STALLS_CHART", "L1i_tlb_stalls: %cycles pipeline stalled due to L1i TLB miss"],
			["OTHER_IQ_DEP_STALL_PCT_CHART", "L1i other stalls: Other instruction queue pipeline stalls not due to icache misses and itlb miss"],
			["ICACHE_BYTES_PER_CYCLE_CHART", "L1i_accesses: 16 bytes * icache_accesses/cycle. Max possible is 16 L1i_bytes/cycle. I might should multiply by 64 bytes/L1i_access"],
			["ITLB_MISS_PER_ICACHE_MISS_CHART", "l1i_tlb_miss/L1i_miss: 100*l1i_tlb_miss/l1i_miss"],
			["ICACHE_MISS_BYTES_PER_CYCLE_CHART", "L1i_misses: 64*L1i_miss/cycle. Max possible is 16 bytes/cycle."],
			["BR_MIS_PRED_PCT_CHART", "%branch_mispredict: 100*branches_mispredicted/branches"]
		];
		t += run_tbl(fe_tbl, txt_tbl, lkup, flds_max, 2, cma, true);

		t += "<tr><td>Execution Engine</td></tr>";
		let ex_tbl = [
			["IPC_CHART", "instructions/cycle"],
			["CPI_CHART", "cycles/instructions"],
			["LD_DEP_STALLS_CHART", "Load_dependent_stalls: pct cycles stalled in the Wr stage because of a load miss."],
			["STALL_SB_FULL_PER_CYCLE_CHART", "Store_buffer_full_stalls: pct of cycles that pipeline is stalled due to store buffer full."],
			["ST_DEP_STALL_PER_CYCLE_CHART", "Store_stalls: pct of cycles pipeline stalled in the Wr stage because of a store."],
			["AGU_DEP_STALLS_CHART", "Address_gen_unit_stalls: pct of cycles pipeline stalled due to a load/store instruction interlocked waiting on data to calculate the address in the AGU"],
			["SIMD_DEP_STALLS_CHART", "SIMD_stalls: pct of cycles there is an interlock for an Advanced SIMD/Floating-point operation."],
			["OTHER_INTERLOCK_STALLS_CHART", "ALU/INT_stalls: pct of cycles there is an interlock other than Advanced SIMD/Floating-point instructions or load/store instruction."],
			["PRE_DECODE_ERR_PER_CYCLE_CHART", "Decoder_stalls: ratio of pre_decode_errors/cycles. Not exactly sure what a pre_decode err is."]
		];
		t += run_tbl(ex_tbl, txt_tbl, lkup, flds_max, 2, cma, true);

		t += "</table>";
		return t;
	}

	function parse_resource_stalls(txt_tbl, hdr_obj)
	{
		if (svg_name == "arm_cortex_a53_block_diagram.svg") {
			let t = parse_resource_stalls_arm_cortex_a53(txt_tbl, hdr_obj);
			return t;
		}
		if (svg_name != "haswell_block_diagram.svg") {
			return "";
		}
		let lkup = {};
		let flds_max = 0;
		g_cpu_diagram_flds.lkup_dvals_for_chart_tag = {};
		for (let i=0; i < txt_tbl.length; i++) {
			let j  = txt_tbl[i].jidx;
			let ct = g_cpu_diagram_flds.cpu_diagram_fields[j].chart_tag;
			if (typeof lkup[ct] === 'undefined') {
				g_cpu_diagram_flds.lkup_dvals_for_chart_tag[ct] = [j];
			} else {
				g_cpu_diagram_flds.lkup_dvals_for_chart_tag[ct].push(j);
			}
			lkup[ct] = i;
			let mx = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length;
			let ci = g_cpu_diagram_flds.cpu_diagram_fields[j].chrt_idx;
			let cd = gcanvas_args[ci][2]; // chart_data
			if (typeof cd.tot_line_opts_xform === 'undefined' || cd.tot_line_opts_xform != 'select_vars') {
				if (flds_max < mx) {
					flds_max = mx;
				}
			}
		}
		let cma = {"t":""};

		cma = {"t":""};
		let cpu_typ_str = "Haswell";
		let sys_tbl = [
			["PCT_BUSY_BY_SYSTEM", "Percent busy system"]
			,["DISK_BW_BLOCK_RQ", "Disk bandwidth"]
			,["TEMPERATURE_BY_ZONE", "temperature (degrees C) by zone"]
			,["FREQ_BY_CPU", "Frequency in GHz by CPU"]
			,["RAPL_POWER_BY_AREA", "Power (W) by chip area"]
			,["BW_BY_TYPE", "Memory bandwidth (MB/sec) by type"]
			];
		let flds_max_sys = -1;
		for (let k=0; k < sys_tbl.length; k++) {
			let ct = sys_tbl[k][0];
			if (typeof lkup[ct] === 'undefined') {
				continue;
			}
			let i = lkup[ct];
			let j  = txt_tbl[i].jidx;
			//let ct = g_cpu_diagram_flds.cpu_diagram_fields[j].chart_tag;
			let mx = g_cpu_diagram_flds.cpu_diagram_fields[j].data_val_arr.length;
			if (flds_max_sys < mx) {
				flds_max_sys = mx;
			}
		}
		let t = "";
		t += "<table border='1' style='float: left'>";
		t += "<tr><td>"+cpu_typ_str+" system metrics</td></tr>";
		t += "<tr><td>Phase (time abs secs)</td><td align='right'>"+sprintf("%.3f", hdr_obj.tm0)+"</td><td align='right'>"+sprintf("%.3f", hdr_obj.tm1)+"</td>";
		for (let i=0; i < flds_max_sys-2; i++) {
			t += "<td></td>";
		}
		let pht = "<td>"+hdr_obj.ph0;
		if (hdr_obj.ph0 != hdr_obj.ph1) {
			pht += " - " + hdr_obj.ph1;
		}
		pht += "</td></tr>";
		t += pht;
		t += run_tbl(sys_tbl, txt_tbl, lkup, flds_max_sys, 2, cma, true);

		t += "</table>";
		let OS_view_hash = {};
		let OS_view_tbl = [];
		for (let j=0; j < g_cpu_diagram_flds.cpu_diagram_fields.length; j++) {
			if (typeof g_cpu_diagram_flds.cpu_diagram_fields[j].save_image_nm === 'undefined') {
				// if save_image_nm not found then this chart can't be used in the OS_view charts
				continue;
			}
			let ct = g_cpu_diagram_flds.cpu_diagram_fields[j].chart;
			if (ct in OS_view_hash) {
				continue;
			}
			OS_view_hash[ct] = j;
			OS_view_tbl.push([ct, j, "desc: "+ct]);
		}

		t += "<table border='1' style='float: left'>";
		t += "<tr><td>"+cpu_typ_str+" OS view metrics</td></tr>";
		t += run_OS_view_tbl(OS_view_tbl);
		t += "</table>";
		t += "<table border='1' style='float: left'>";
		t += "<tr><td>"+cpu_typ_str+" CPU diagram metrics</td>";
		for (let i=0; i < flds_max; i++) {
			t += "<td title='Value of the computed metric for core or system'>core"+i+" val</td>";
		}
		for (let i=0; i < flds_max; i++) {
			t += "<td title='Some of these are guesses as to whether the core is stalled.'>core"+i+" stalled</td>";
		}
		t += "<td>Resource utilization description</td></tr>";
		t += "<tr><td>Memory Subsystem </td></tr>";

		let mem_tbl = [
			["UNC_HASWELL_PEAK_BW_CHART", "DRAM: pct of max possible memory BW 25.9 GB/s"],
			["L3_MISS_BYTES_PER_CYCLE_CHART", "L3_misses: max L3 misses about 10 bytes/cycle"],
			["SQ_FULL_PER_CYCLE_CHART", "L2_misses: pct of cycles SuperQueue full so can't take more requests"],
			["L2_MISS_BYTES_PER_CYCLE_CHART", "L2_misses: path to L3 64 bytes/cycle max"],
			["L2_TRANS_ALL_PER_CYCLE_CHART", "L2_traffic: path L1d to L2 64 bytes/cycle max"],
			["ICACHE_MISS_BYTES_PER_CYCLE_CHART", "L2_traffic: path icache to L2 64 bytes/cycle max"],
			["L1D_PENDING_MISS_FB_FULL_PER_TOT_CYCLES_CHART", "FB stalls: pct cycles L1d demand request blocked due to Fill Buffer (FB) full"],
			["L1D_REPL_BYTES_PER_CYCLE_CHART", "L1d: L1d to load buffer: 2 paths of 32 bytes/cycle"],
			["RESOURCE_STALLS.SB_PER_TOT_CYCLES_CHART", "L1d: L1d to store buffer: %cycles stalled due store buffer full"]
		];
		t += run_tbl(mem_tbl, txt_tbl, lkup, flds_max, 2, cma, true);

		t += "<tr><td>Front End</td></tr>";

		let fe_tbl = [
			["IFETCH_STALLS_PER_TOT_CYCLES_CHART", "L1i: %cycles stalled due to L1 icache miss"],
			["MISPRED_CYCLES_PER_TOT_CYCLES_CHART", "Branch prediction Unit: %cycles allocator stalled due to mispredict or memory nuke"],
			["DSB_UOPS_PER_CYCLE_CHART", "Decode Stream Buffer: uops to Instr Decode Queue IDQ from DSB path per cycle. Max possible is 4 uops/cycle"],
			["IDQ_ALL_UOPS_PER_CHART_CHART", "IDQ: uop to IDQ from MITE path/cycle. Max possible is 4 uops/cycle"],
			["LSD_UOPS_PER_CYCLE_CHART", "LSD: Loop Stream Detector uop to IDQ/cycle. Max possible is 4 uops/cycle. "],
			["MS_UOPS_PER_CYCLE_CHART", "MS_UOPS: Microcode Sequencer uops to mux. Max possible is 4 uops/cycle. "]
		];
		t += run_tbl(fe_tbl, txt_tbl, lkup, flds_max, 2, cma, true);

		t += "<tr><td>Execution Engine</td></tr>";
		let ex_tbl = [
			["UOPS_ISSUED.CORE_STALL_CYCLES_PER_TOT_CYCLES_CHART", "RAT: pct of total cycles where uops not issued by Register Allocation Table", 2],
			["UOPS_RETIRED.CORE_STALL_CYCLES_PER_TOT_CYCLES_CHART", "EXE: pct of total cycles where uops not retired", 2],
			["RESOURCE_STALLS.RS_PER_TOT_CYCLES_CHART", "RS: pct of total cycles stalled due to no eligible Reservation Station entry available"],
			["RAT_UOPS_PER_CYCLE_CHART", "RAT:  RAT uop issued/cycle. Max possible is 4 uops issued/cycles. Compare to the DSB, LSD, IDQ uops/cycle to see which path is probably mostly feeding the RAT", 2]
		];
		t += run_tbl(ex_tbl, txt_tbl, lkup, flds_max, 2, cma, true);

		let uop_tbl = [];
		for (let i=0; i < 8; i++) {
			uop_tbl.push(["UOPS_port_"+i+"_PER_CYCLE_CHART", "port"+i+": uops/cycle on port"+i+". Bigger values means port used more"]);
		}
		for (let i=0; i < 8; i++) {
			uop_tbl.push(["GUOPS_port_"+i+"_CHART", "port"+i+": uops/nsec on port"+i+". Small values may mean port is not used much"]);
		}
		t += run_tbl(uop_tbl, txt_tbl, lkup, flds_max, 4, cma, false);
		t += "</table>";
		return t;
	}

	function os_cpu_can_overlap(x, y, doing_click)
	{
		let current_tooltip_text = "aa";
		let arr3 = find_overlap(x, y);
		if (arr3.length == 0) {
			//clearToolTipText(mytooltip);
			current_tooltip_text = "";
			tt_prv_x = -2;
			tt_prv_y = -2;
			mycanvas.title = "";
			//current_tooltip_shape = -1;
			return;
		}
		let arr = [];
		for (let j=0; j < arr3.length; j++) {
			arr.push(arr3[j].j);
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
				if (typeof arr3[j].fig !== 'undefined' && typeof arr3[j].img_idx !== 'undefined') {
					//console.log(sprintf("got ui= %d, fig[%d]", ui, arr3[j].fig));
					let tbox, img_shape, doing_flame=false;
					if (typeof g_cpu_diagram_flds.figures[arr3[j].fig].figure.cmds[arr3[j].cmds].tbox_fl !== 'undefined') {
						tbox = g_cpu_diagram_flds.figures[arr3[j].fig].figure.cmds[arr3[j].cmds].tbox_fl;
						img_shape =  gjson.chart_data[tbox.img_idx].fl_image_shape;
						doing_flame = true;
					} else if (typeof g_cpu_diagram_flds.figures[arr3[j].fig].figure.cmds[arr3[j].cmds].tbox_img !== 'undefined') {
						tbox = g_cpu_diagram_flds.figures[arr3[j].fig].figure.cmds[arr3[j].cmds].tbox_img;
						img_shape =  gjson.chart_data[tbox.img_idx].image_data.shape;
						//console.log(sprintf("ele_nm= %s", img_shape.ele_title_nm));
						if (doing_click) {
							let ele = document.getElementById(img_shape.ele_title_nm);
							if (ele !== null) {
								let ttl = ele.innerHTML;
								console.log(sprintf("ttl= "+ttl));
								ele.scrollIntoView(true);
							}
						}
					} else {
						continue;
					}
					/*
					console.log(sprintf("ck tbox fig[%d].cmds[%d] x= %.2f, y= %.2f, xb= %.2f, xe= %.2f, yb= %.2f, ye= %.2f"+
							"\nshape_orig: hi= %.2f, wd= %.2f, scaled_img: hi= %.2f, wd: %.2f",
							arr3[j].fig, arr3[j].cmds, x, y, tbox.xb, tbox.xe, tbox.yb, tbox.ye,
							img_shape.high, img_shape.wide, tbox.ye-tbox.yb, tbox.xe-tbox.xb));
					*/
					/*
					mycanvas.title = sprintf("flame graph x= %.2f, y= %.2f\n"+
							"shape_orig: hi= %.2f, wd= %.2f, scaled_img: hi= %.2f, wd: %.2f",
							x-tbox.xb, y-tbox.yb, img_shape.high, img_shape.wide, tbox.ye-tbox.yb, tbox.xe-tbox.xb);
							*/
					let ele_ttl = "";
					let ele = document.getElementById(img_shape.ele_title_nm);
					if (ele !== null) {
						ele_ttl = ele.innerHTML;
						ele_ttl = ele_ttl.replace("<b>", "");
						//<b>lnx_arm_multi10</b>&nbsp;syscalls: call (by end of call) per interval
						ele_ttl = ele_ttl.replace("</b>", "");
						ele_ttl = ele_ttl.replace("&nbsp;", " ");
						ele_ttl = "title: " + ele_ttl + "\n";
						//console.log(sprintf("ttl= "+ele_ttl));
						if (doing_click) {
							ele.scrollIntoView(true);
						}
					}
					let xoff = x-tbox.xb;
					let yoff = y-tbox.yb;
					let xscl = img_shape.wide/(tbox.xe-tbox.xb);
					let yscl = img_shape.high/(tbox.ye-tbox.yb);
					let tstr = null;
					if (doing_flame) {
						tstr = gjson.chart_data[tbox.img_idx].fl_image_hover(xoff*xscl, yoff*yscl);
					} else {
						tstr = gjson.chart_data[tbox.img_idx].image_data.hover(xoff*xscl, yoff*yscl, 2.0*xscl, 2.0*yscl);
					}
					if (typeof tstr !== 'undefined' && typeof tstr.str2 !== 'undefined') {
						let ttl_str = ele_ttl + tstr.str2;
						ttl_str = ttl_str.replace(/<br>/g, "\n");
						mycanvas.title = ttl_str;
					} else {
						mycanvas.title = "";
					}
					continue;
				}
				if (typeof g_cpu_diagram_flds.cpu_diagram_fields[ui].grf_def !== 'undefined') {
					let uj = ui;
					let um = arr3[j].m;
					if (g_cpu_diagram_flds.cpu_diagram_fields[ui].grf_def.typ == 'pie') {
						let slc = arr3[j].slc;
						if (slc == -1) {
							console.log(sprintf("messed up angle lookup for slice: grf= %s, ui= %d, um= %d, slc= %d", 
								g_cpu_diagram_flds.cpu_diagram_fields[ui].grf_def.nm, ui, um, slc));
							continue;
						}
						let pies = g_cpu_diagram_flds.cpu_diagram_fields[uj].pies;
						if (typeof pies[um].arr[slc] === 'undefined') {
							console.log(sprintf("grf= %s, ui= %d, um= %d, slc= %d", 
								g_cpu_diagram_flds.cpu_diagram_fields[ui].grf_def.nm, ui, um, slc));
						}
						let pct = sprintf("%.3f%%", pies[um].arr[slc].pct);
						let val_txt = sprintf("%.3f%%", pies[um].arr[slc].value);
						if (pct != val_txt) {
							val_txt = sprintf("%.3f", pies[um].arr[slc].value);
						}
						pct = sprintf("%.3f%%", 100.0 * pies[um].arr[slc].pct);
						let ts= sprintf("%s: %s, %s", pies[um].arr[slc].lbl, val_txt, pct);
						console.log(ts);
						mycanvas.title = ts;
					}
					if (g_cpu_diagram_flds.cpu_diagram_fields[ui].grf_def.typ == 'vbar') {
						let um = arr3[j].m;
						let bars = g_cpu_diagram_flds.cpu_diagram_fields[ui].bars;
						//console.log("bars[m].txt1= "+ bars[um].arr[0].evt_str);
						//mycanvas.title = "bar55";
						mycanvas.title = g_cpu_diagram_flds.cpu_diagram_fields[ui].grf_def.nm + " " +
							bars[um].arr[0].evt_str + ": " + sprintf("%.3f", bars[um].arr[0].value) + 
							sprintf(", pct_max= %.3f%%", bars[um].arr[0].pct);
					}
				} else {
					let ts= g_cpu_diagram_flds.cpu_diagram_fields[ui].y_label+ ": " +
						g_cpu_diagram_flds.cpu_diagram_fields[ui].tbox.txt+',\n'+
						g_cpu_diagram_flds.cpu_diagram_fields[ui].desc;
					console.log(ts);
					if (tt_prv_x != x || tt_prv_y != y) {
						tt_prv_x = x;
						tt_prv_y = y;
						//let pgx = e.pageX, pgy = e.pagey;
					//console.log(sprintf("tooltip txt= %s, x= %.3f, y= %.3f, ttl= %s, ttr=%s ttt= %s ttb= %s",ts, x, y,
					//mytooltip.style.left, mytooltip.style.right, mytooltip.style.top, mytooltip.style.bottom));
						mycanvas.title = ts;
						//console.log(mytooltip);
					}
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
				typeof shapes[i].is_connector !== 'undefined' &&
				shapes[i].is_connector == true ) {
				//console.log(shapes[i]);
				for (let j=0; j < shapes[i].poly.length; j++) {
					if (typeof shapes[i].poly[j].im_connected_to !== 'undefined') {
						let arr2 = shapes[i].poly[j].im_connected_to;
						for (let k=0; k < arr2.length; k++) {
							if (arr.indexOf(arr2[k]) == -1) {
								arr.push(arr2[k]);
							}
						}
					}
				}
				if (typeof shapes[i].is_connected_to_me !== 'undefined') {
					for (let m=0; m < shapes[i].is_connected_to_me.length; m++) {
						let cf = shapes[i].is_connected_to_me[m];
						if (
							shapes[cf].typ == 'path' &&
							typeof shapes[cf].is_connector !== 'undefined' &&
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
			draw_svg(arr, -1, -1);
			did_svg_highlight = true;
		} else {
			if (did_svg_highlight) {
				draw_svg([], -1, -1);
				did_svg_highlight = false;
			}
		}
		return;
	}

	mycanvas.onmouseup = function (evt) {
		let rect = this.getBoundingClientRect(),
			x = (evt.clientX - rect.left),
			y = (evt.clientY - rect.top);
		let y2 = y, sctr = 0;
		if (y > y_shift) {
			sctr = 1;
			y2 = y - y_shift;
		}
		console.log(sprintf("x= %.3f, y= %.3f, sctr= %d, y2= %.3f", x, y, sctr, y2));
		os_cpu_can_overlap(x, y, true);
	}

	mycanvas.onmousemove = function(e) {
		// important: correct mouse position:
		let rect = this.getBoundingClientRect(),
			x = e.clientX - rect.left,
			y = e.clientY - rect.top;
		if (x == hvr_prv_x && y == hvr_prv_y) {
			return;
		}
		hvr_prv_x = x;
		hvr_prv_y = y;
		os_cpu_can_overlap(x, y, false);
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

	draw_svg([], -1, -1);

	console.log("got to end of parse_svg()");
}
