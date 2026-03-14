/*
 * Function Calling Example
 * 
 * Demonstrates tool use / function calling with the DashScope API.
 * This example defines three tools (get_weather, get_current_time, get_date)
 * and allows the model to invoke them based on user requests.
 */

#define DASHSCOPE_IMPLEMENTATION
#include "dashscope.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Retrieve environment variable by name
 * 
 * Exits with code 1 if the variable is not set.
 */
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
		.model = DS_MODEL_QWEN_PLUS,
		.stream = false
	};

	DS_Content system_prompt = ds_content_text("You are a helpful assistant with access to tools.");
	DS_Content user_input = ds_content_text("What's the weather in Beijing and the current time?");
	
	DS_Message messages[] = {
		{ .role = DS_MESSAGE_ROLE_SYSTEM, .content = &system_prompt, .content_count = 1 },
		{ .role = DS_MESSAGE_ROLE_USER, .content = &user_input, .content_count = 1 }
	};

	/* Define tools */
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

	DS_Tool get_date_tool = {
		.type = DS_TOOL_TYPE_FUNCTION,
		.function = {
			.name = "get_date",
			.description = "Get the current date.",
			.parameters = {
				.type = DS_PARAMETER_TYPE_OBJECT,
				.properties = NULL,
				.property_count = 0,
				.required_properties = NULL,
				.required_property_count = 0
			}
		}
	};

	DS_Tool tools[] = { get_weather_tool, get_current_time_tool, get_date_tool };

	DS_Response *response = ds_generate(&generation, messages, 2, tools, 3);

	if (response && response->output.choice_count > 0) {
		DS_Choice *choice = &response->output.choices[0];
		
		if (choice->message_count > 0 && choice->message[0].content_count > 0) {
			printf("Response: %s\n", choice->message[0].content[0].text);
		}
		
		if (choice->tool_call_count > 0) {
			printf("\nTool Calls:\n");
			for (size_t i = 0; i < choice->tool_call_count; ++i) {
				DS_ToolCall *call = &choice->tool_calls[i];
				printf("  [%zu] %s(%s)\n", i + 1, call->name, call->arguments);
			}
		}
		
		printf("\nFinish Reason: %d\n", choice->finish_reason);
	}

	if (response) {
		printf("Usage: input_tokens=%zu, output_tokens=%zu, total_tokens=%zu\n",
			response->output.usage.input_tokens,
			response->output.usage.output_tokens,
			response->output.usage.total_tokens
		);
	}

	ds_free_response(response);

	return 0;
}
