/* Copyright (c) 2013-2018 the Civetweb developers
 * Copyright (c) 2013 No Face Press, LLC
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

// Based on: Simple example program on how to use Embedded C++ interface.

#include "CivetServer.h"
#include "perf_event_subset.h"
#include <string>
#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <cstring>
#include "utils2.h"
#include "utils.h"
#include "web_api.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

bool exitNow = false;
bool got_quit= false;

enum conn_stat {
	CONN_FREE  = 0,
	CONN_INUSE = 1,
};

struct connected_str {
	int status;
	struct mg_connection *conn;
	connected_str(): status(0), conn(0) {}
};

static std::vector <connected_str> connections;

static Queue<std::string> *q_from_srvr_to_clnt;
static Queue<std::string> *q_bin_from_srvr_to_clnt;
static Queue<std::string> *q_from_clnt_to_srvr;

// if first part of data is == the str at ck_str then return strlen(ck_str)
// else return 0
int ck_cmd(const char *data, int data_len, const char *ck_str)
{
	if (!ck_str) {
		return 0;
	}
	int nlen = strlen(ck_str);
	if (data_len >= nlen) {
		if (memcmp(data, ck_str, nlen) == 0) {
			return nlen;
		}
	}
	return 0;
}

static std::string msg_data; // yes, this needs to be per client.

#ifdef USE_WEBSOCKET
class WebSocketHandler : public CivetWebSocketHandler {

	virtual bool handleConnection(CivetServer *server,
	                              const struct mg_connection *conn) {
		printf("WS connected\n");
		return true;
	}

	virtual void handleReadyState(CivetServer *server,
	                              struct mg_connection *conn) {
		printf("WS ready\n");

		//const char *text = "Hello from the websocket ready handler";
		//mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, text, strlen(text));
		int use_conn = -1;
		for (int i=0; i < (int)connections.size(); i++) {
			if (connections[i].status != CONN_INUSE) {
				connections[i].status = CONN_INUSE;
				connections[i].conn = conn;
				use_conn = i;
				break;
			}
		}
		if (use_conn == -1) {
			connected_str cs;
			cs.conn = conn;
			cs.status = CONN_INUSE;
			connections.push_back(cs);
		}
	}

	virtual bool handleData(CivetServer *server,
	                        struct mg_connection *conn,
	                        int bits,
	                        char *data,
	                        size_t data_len) {
		uint32_t sz = data_len;
		int is_eom   = ((bits & 0x80) != 0);
		int is_text  = ((bits & 0xf) == MG_WEBSOCKET_OPCODE_TEXT);
		int is_bin   = ((bits & 0xf) == MG_WEBSOCKET_OPCODE_BINARY);
		int is_ping  = ((bits & 0xf) == MG_WEBSOCKET_OPCODE_PING);
		int is_pong  = ((bits & 0xf) == MG_WEBSOCKET_OPCODE_PONG);
		int is_close = ((bits & 0xf) == MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE);
		int is_cont  = ((bits & 0xf) == MG_WEBSOCKET_OPCODE_CONTINUATION);
		printf("WS got %lu bytes: ", (long unsigned)data_len);
		if (sz > 100) {
			sz = 100;
		}
		printf("msg bits= 0x%x is_eom= %d is_text= %d, bin= %d, ping= %d, pong= %d, close= %d cont= %d at %s %d\n",
				bits, is_eom, is_text, is_bin, is_ping, is_pong, is_close, is_cont, __FILE__, __LINE__);
		if(is_cont) {
			//printf("got cont image data_len= %d from client at %s %d\n", (int)(data_len), __FILE__, __LINE__);
			std::string msg;
			msg = data;
			msg = msg.substr(0, data_len);
			msg_data += msg;
			if (is_eom) {
				q_from_clnt_to_srvr->push(msg_data);
				msg_data = "";
			}
			return 1;
		}
		if (sz > 2) {
			fwrite(data, 1, sz, stdout);
			printf("\n");
		}
		if(ck_cmd(data, data_len, "msg_frm_clnt=") > 0) {
			printf("got cmd= '%s' at %s %d\n", data, __FILE__, __LINE__);
		}
		if(ck_cmd(data, data_len, "int=") > 0) {
			int i = atoi(data+4);
			printf("got int from client= %d at %s %d\n", i, __FILE__, __LINE__);
			q_from_clnt_to_srvr->push(std::to_string(i+1000));
			return 1;
		}
		if(ck_cmd(data, data_len, "Ready") > 0) {
			printf("got Ready from client at %s %d\n", __FILE__, __LINE__);
			q_from_clnt_to_srvr->push("Ready");
			return 1;
		}
		if(ck_cmd(data, data_len, "image,") > 0 || ck_cmd(data, data_len, "imagd,") > 0) {
			printf("got image data_len= %d from client at %s %d\n", (int)(data_len), __FILE__, __LINE__);
			std::string msg;
			msg = data;
			msg = msg.substr(0, data_len);
			if (is_eom) {
				q_from_clnt_to_srvr->push(msg);
			} else {
				msg_data = msg; // yes, this needs to be per client.
			}
			return 1;
		}
		if(ck_cmd(data, data_len, "json_table=") > 0) {
			printf("got json_table data_len= %d from client at %s %d\n", (int)(data_len), __FILE__, __LINE__);
			std::string msg;
			msg = data;
			msg = msg.substr(0, data_len);
			if (is_eom) {
				q_from_clnt_to_srvr->push(msg);
			} else {
				msg_data = msg; // yes, this needs to be per client.
			}
			return 1;
		}
		if(ck_cmd(data, data_len, "parse_svg") > 0) {
			std::string msg;
			printf("got parse_svg from client at %s %d\n", __FILE__, __LINE__);
			msg = data;
			msg = msg.substr(0, data_len);
			q_from_clnt_to_srvr->push(msg);
			return 1;
		}
		if(ck_cmd(data, data_len, "quit") > 0) {
			printf("got cmd= '%s' at %s %d\n", data, __FILE__, __LINE__);
			exitNow = true;
			got_quit= true;
			return 0;
		}
#if 0
		if (!(q_from_srvr_to_clnt->is_empty())) {
			int for_clnt = q_from_srvr_to_clnt->pop();
			std::string str;
			str = "data from server: " + std::to_string(for_clnt);
			fwrite(str.c_str(), 1, str.size(), stdout);
			mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, str.c_str(), str.size());
		}
#endif
		return 1;
		//return (data_len<2);
	}

	virtual void handleClose(CivetServer *server,
	                         const struct mg_connection *conn) {
		printf("WS closed\n");
		for (int i=0; i < (int)connections.size(); i++) {
			if (connections[i].conn == conn) {
				connections[i].status = CONN_FREE;
				connections[i].conn = NULL;
				//use_conn = i;
				break;
			}
		}
	}
};
#endif


#if 1
//int civetweb_main(int argc, char *argv[])
int civetweb_main(Queue<std::string>& q_from_srvr, Queue<std::string>& q_bin_from_srvr, Queue<std::string>& q_from_clnt, int *thrd_status, std::vector <std::string> cw_argv, int port, int verbose)
#else
int
main(int argc, char *argv[])
#endif
{
	q_from_srvr_to_clnt = &q_from_srvr;
	q_bin_from_srvr_to_clnt = &q_bin_from_srvr;
	q_from_clnt_to_srvr = &q_from_clnt;

	std::vector<std::string> cpp_options;
    std::string ds = std::string(DIR_SEP);
	std::string doc_root, base_file = "web" + ds;
	std::vector <std::string> tried_names;
	int rc = search_for_file(base_file, doc_root, tried_names, __FILE__, __LINE__, verbose);
	if (rc != 0) {
		search_for_file(base_file, doc_root, tried_names, __FILE__, __LINE__, 1);
		fprintf(stderr, "bye at %s %d\n", __FILE__, __LINE__);
		exit(1);
	}
	printf("found web dir %s using name= %s at %s %d\n", base_file.c_str(), doc_root.c_str(), __FILE__, __LINE__);
    cpp_options.push_back("document_root");
    cpp_options.push_back(doc_root);
    cpp_options.push_back("listening_ports");
    cpp_options.push_back("127.0.0.1:"+std::to_string(port));
    cpp_options.push_back("num_threads");
    cpp_options.push_back("5");
    cpp_options.push_back("enable_websocket_ping_pong");
    cpp_options.push_back("yes");
	printf("civetweb options at %s %d\n", __FILE__, __LINE__);
	for (uint32_t i=0; i < cpp_options.size(); i++) {
		printf("option arg[%d]: %s\n", i, cpp_options[i].c_str());
	}

	CivetServer server(cpp_options); // <-- C++ style start



#ifdef USE_WEBSOCKET
	WebSocketHandler h_websocket;
	//server.addWebSocketHandler("/websocket", h_websocket);
	server.addWebSocketHandler("/", h_websocket);
	printf("Connect to websocket at http://localhost:%d\n", port);
#endif

	*thrd_status = THRD_RUNNING;

	while (!exitNow) {
#ifdef _WIN32
		Sleep(200);
#else
		usleep(100*1000);
#endif
#if 1
		for (int i=0; i < (int)connections.size(); i++) {
			if (connections[i].status != CONN_INUSE) {
				continue;
			}
			if (!(q_from_srvr_to_clnt->is_empty())) {
				std::string for_clnt = q_from_srvr_to_clnt->pop();
				//std::string str;
				//str = "send_int=" + for_clnt;
				mg_websocket_write(connections[i].conn, MG_WEBSOCKET_OPCODE_TEXT, for_clnt.c_str(), for_clnt.size());
				//str += "\n";
				//fwrite(for_clnt.c_str(), 1, for_clnt.size(), stdout);
			}
			if (!(q_bin_from_srvr_to_clnt->is_empty())) {
				std::string for_clnt = q_bin_from_srvr_to_clnt->pop();
				//std::string str;
				//str = "send_int=" + for_clnt;
				mg_websocket_write(connections[i].conn, MG_WEBSOCKET_OPCODE_BINARY, for_clnt.c_str(), for_clnt.size());
				//str += "\n";
				//fwrite(for_clnt.c_str(), 1, for_clnt.size(), stdout);
			}
		}
		if (get_signal() != 0) {
			break;
		}
#endif
	}

	if (got_quit) {
		set_signal();
	}


	printf("Bye from embedded_cpp thead!\n");

	return 0;
}
