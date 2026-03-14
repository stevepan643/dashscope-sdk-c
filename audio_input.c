/*
 * Audio Input Example
 * 
 * Demonstrates audio processing with the DashScope API.
 * This example sends an audio file to the Qwen2 Audio Instruct model
 * for speech understanding and transcription/analysis.
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
		.model = DS_MODEL_QWEN2_AUDIO_INSTRUCT,
		.stream = false
	};

	DS_Content system_prompt = ds_content_text("You are a helpful assistant!");
	
	DS_Content user_contents[] = {
		ds_content_text("What is in this audio?"),
		ds_content_audio("https://dashscope.oss-cn-beijing.aliyuncs.com/audios/welcome.mp3")
	};
	
	DS_Message messages[] = {
		{ .role = DS_MESSAGE_ROLE_SYSTEM, .content = &system_prompt, .content_count = 1 },
		{ .role = DS_MESSAGE_ROLE_USER, .content = user_contents, .content_count = 2 }
	};

	DS_Response *response = ds_generate(&generation, messages, 2, NULL, 0);

	if (response && response->output.choice_count > 0) {
		DS_Choice *choice = &response->output.choices[0];
		if (choice->message_count > 0 && choice->message[0].content_count > 0) {
			printf("Response: %s\n", choice->message[0].content[0].text);
		}
		printf("Finish Reason: %d\n", choice->finish_reason);
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
