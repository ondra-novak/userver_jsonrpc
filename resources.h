#pragma once

#include <string_view>

namespace userver {


struct Resource {
	std::string_view contentType;
	std::string_view data;
};



extern Resource client_index_html;
extern Resource client_rpc_js;
extern Resource client_styles_css;

}
