#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

bool rapid_ck_json(std::string &json, std::string from_where, const char *file, int line)
{
	rapidjson::Document d;
	if (d.Parse(json.c_str()).HasParseError()) {
    	fprintf(stderr, "\njson parse from %s called by %s %d: at %s %d: Error(offset %u): %s\n", 
				from_where.c_str(), file, line,
				__FILE__, __LINE__,
        	(unsigned)d.GetErrorOffset(),
        	GetParseError_En(d.GetParseError()));
    	// ...
		return false;
	} else {
		return true;
	}
}
