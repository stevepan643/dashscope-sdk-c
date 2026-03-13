#define DASHSCOPE_IMPLEMENTATION
#include "dashscope.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODEL_NAME	"qwen-plus"

static inline char *get_env(const char *name) {
	const char *value = getenv(name);
	if (!value) {
		fprintf(stderr, "Environment variable %s not set\n", name);
		exit(1);
	}
	return strdup(value);
}

int main(int argc, char **argv)
{
	DS_Generation generation = {
		.api_key = get_env("DASHSCOPE_API_KEY"),
		.model_name = MODEL_NAME,
		.stream = false
	};

	DS_Content contents[] = {
		ds_content_text("You are a helpful assistant!"),
		ds_content_text("Hello, Qwen! What's the weather like in New York? And what's the current time?"),
	};
	
	DS_Message messages[] = {
		{ .role = DS_MESSAGE_ROLE_SYSTEM, .content = &contents[0], .content_count = 1 },
		{ .role = DS_MESSAGE_ROLE_USER, .content = &contents[1], .content_count = 1 }
	};

	DS_Tool get_weather_tool = {
		.type = DS_TOOL_TYPE_FUNCTION,
		.function = {
			.name = "get_weather",
			.description = "Get the current weather for a given location.",
			.parameters = {
				.type = DS_PARAMETER_TYPE_OBJECT,
				.properties = (DS_Property[]) {
					{ .type = DS_PROPERTY_TYPE_STRING, .name = "location", .description = "The location to get the weather for." }
				},
				.property_count = 1,
				.required_properties = (const char*[]) { "location" },
				.required_property_count = 1
			}
		}
	};

	DS_Tool get_current_time_tool = {
		.type = DS_TOOL_TYPE_FUNCTION,
		.function = {
			.name = "get_current_time",
			.description = "Get the current time.",
			.parameters = {
				.type = DS_PARAMETER_TYPE_OBJECT,
				.properties = NULL,
				.property_count = 0,
				.required_properties = NULL,
				.required_property_count = 0
			}
		}
	};

	DS_Response *response = ds_generate(&generation, messages, 2, (DS_Tool[]) { get_weather_tool, get_current_time_tool }, 2);

	if (response != NULL && response->output.choice_count > 0) {
		if (response->output.choices->finish_reason == DS_FINISH_REASON_TOOL_CALL) {
			printf("Tool call: %s\n", response->output.choices->tool_calls[0].name);
		} else {
			printf("Message: %s\n", response->output.choices[0].message[0].content[0].text);
		}
	} else {
		printf("No response generated.\n");
	}

	ds_free_response(response);

	printf("Key: %s\n", generation.api_key);

	return 0;
}
