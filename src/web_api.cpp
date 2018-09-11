/* Copyright (c) 2018 Patrick Fay
 *
 * License http://opensource.org/licenses/mit-license.php MIT License
 */

#include <queue>
#include <mutex>
#include <string>
#include <stdio.h>
#include "web_api.h"

void push_to_client( Queue <std::string> & q_from_srvr, int i) {
	printf("push i= %d\n", i);
	q_from_srvr.push(std::to_string(i));
}

