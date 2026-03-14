# DashScope SDK for C

A lightweight, dependency-minimal C SDK for interacting with Alibaba DashScope API. Supports text generation, multimodal processing (images, videos, audio), and function calling.

## Features

- **Text Generation**: Query the Qwen Plus model for text completion and reasoning
- **Multimodal Input**: Process images, videos, and audio alongside text
- **Function Calling**: Define custom tools and let the model decide when to invoke them
- **Automatic JSON Handling**: Built-in request/response serialization with cJSON
- **Memory Safety**: Automatic cleanup helpers for all allocated resources
- **Single Header Implementation**: Include and compile, no complex setup required

## Supported Models

- `DS_MODEL_QWEN_PLUS` - Fast text generation model
- `DS_MODEL_QWEN_VL_PLUS` - Vision-language model supporting images and video
- `DS_MODEL_QWEN_VL_MAX_LATEST` - Advanced vision-language model with streaming
- `DS_MODEL_QWEN2_AUDIO_INSTRUCT` - Audio processing and speech understanding

## Requirements

- GCC or Clang (C99 standard)
- libcurl for HTTP requests
- cJSON for JSON processing

### Installation (Ubuntu/Debian)

```bash
sudo apt-get install libcurl4-openssl-dev libcjson-dev
```

### Installation (macOS)

```bash
brew install curl cjson
```

## Quick Start

### Setup

1. Clone or download this repository
2. Set your DashScope API key as an environment variable:

```bash
export DASHSCOPE_API_KEY="your-api-key-here"
```

### Build All Examples

```bash
make
```

Or compile manually:

```bash
gcc -o text_input text_input.c -lcurl -lcjson
```

### Run Examples

```bash
make run EXAMPLE=text_input
./text_input
```

## Examples

### Text Generation

See `text_input.c` for a basic example:

```c
DS_Generation generation = {
    .api_key = get_env("DASHSCOPE_API_KEY"),
    .model = DS_MODEL_QWEN_PLUS,
    .stream = false
};

DS_Content prompt = ds_content_text("What is AI?");
DS_Message messages[] = {
    { .role = DS_MESSAGE_ROLE_SYSTEM, .content = &system_prompt, .content_count = 1 },
    { .role = DS_MESSAGE_ROLE_USER, .content = &prompt, .content_count = 1 }
};

DS_Response *response = ds_generate(&generation, messages, 2, NULL, 0);

if (response && response->output.choice_count > 0) {
    printf("Response: %s\n", response->output.choices[0].message[0].content[0].text);
}

ds_free_response(response);
```

### Image Analysis

See `image_input.c`:

```c
DS_Content user_contents[] = {
    ds_content_text("Describe these images:"),
    ds_content_image("https://example.com/image1.jpg"),
    ds_content_image("https://example.com/image2.jpg")
};
```

### Video Analysis

See `video_input.c`:

```c
const char *frames[] = {
    "https://example.com/frame1.jpg",
    "https://example.com/frame2.jpg",
    "https://example.com/frame3.jpg"
};

DS_Content user_contents[] = {
    ds_content_text("Analyze this video:"),
    ds_content_video(frames, 3)
};
```

### Audio Processing

See `audio_input.c`:

```c
DS_Content user_contents[] = {
    ds_content_text("What is in this audio?"),
    ds_content_audio("https://example.com/audio.mp3")
};
```

### Function Calling

See `function_call.c`:

```c
DS_Tool tools[] = {
    {
        .type = DS_TOOL_TYPE_FUNCTION,
        .function = {
            .name = "get_weather",
            .description = "Get the current weather",
            .parameters = {
                .type = DS_PARAMETER_TYPE_OBJECT,
                .properties = (DS_Property[]) {
                    { .type = DS_PROPERTY_TYPE_STRING, .name = "location", .description = "City name" }
                },
                .property_count = 1,
                .required_properties = (const char*[]) { "location" },
                .required_property_count = 1
            }
        }
    }
};

DS_Response *response = ds_generate(&generation, messages, 2, tools, 1);

if (response && response->output.choices[0].tool_call_count > 0) {
    DS_ToolCall *call = &response->output.choices[0].tool_calls[0];
    printf("Model called: %s(%s)\n", call->name, call->arguments);
}
```

## API Reference

### Core Types

- `DS_Model` - Available models (enum)
- `DS_ModelCapability` - Model capability flags
- `DS_Generation` - Generation configuration (API key, model, streaming)
- `DS_Content` - Content item (text, image, video, audio)
- `DS_Message` - Message with role and content
- `DS_Response` - API response with choices and usage
- `DS_ToolCall` - Invoked function name and arguments

### Main Functions

```c
/*
 * Generate response from messages
 * Returns dynamically allocated DS_Response* (must be freed)
 */
DS_Response *ds_generate(
    const DS_Generation *generation,
    const DS_Message *messages,
    size_t message_count,
    const DS_Tool *tools,
    size_t tool_count
);

/*
 * Free response and all nested allocations
 */
void ds_free_response(DS_Response *response);
```

### Content Constructors

```c
/*
 * Create content objects (no allocation, store references)
 */
DS_Content ds_content_text(const char *text);
DS_Content ds_content_image(const char *url);
DS_Content ds_content_video(const char **frames, size_t count);
DS_Content ds_content_audio(const char *url);
```

## Implementation Details

- **Single Header**: All implementation is in `dashscope.h` with `#define DASHSCOPE_IMPLEMENTATION`
- **No Global State**: Completely stateless, thread-safe for independent requests
- **Memory Management**: All response data allocated with malloc/calloc, freed with provided helpers
- **JSON Parsing**: Uses cJSON for robust request/response handling
- **HTTP Over libcurl**: Supports SSL/TLS and follows redirect standards

## Building a Custom Application

1. Copy `dashscope.h` to your project
2. In your main source file, define the implementation:

```c
#define DASHSCOPE_IMPLEMENTATION
#include "dashscope.h"
```

3. Compile with libcurl and cJSON:

```bash
gcc -o myapp myapp.c -lcurl -lcjson
```

## Version

**0.1.0** (2026-03-14)

Initial release with complete text generation, multimodal, and function calling support.

## License

Apache 2.0 License - Feel free to use, modify, and distribute

## Support

For API documentation, visit: [DashScope API Documentation](https://dashscope.aliyun.com/)

For issues with this SDK, please check the examples and ensure:
- Your API key is valid and has proper permissions
- You have network connectivity to the DashScope endpoint
- Required dependencies (libcurl, cJSON) are installed

## Roadmap

- [ ] Streaming response support for long-form generation
- [ ] Async callback-based request handling
- [ ] Connection pooling and request batching
- [ ] Built-in retry logic and exponential backoff
- [ ] Example server implementation
