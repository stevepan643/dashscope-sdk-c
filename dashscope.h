#ifndef DASHSCOPE_H
#define DASHSCOPE_H

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    const char *api_key;
    const char *model_name;

    bool stream;
} DS_Generation;

typedef enum {
    DS_CONTENT_TYPE_TEXT,
    DS_CONTENT_TYPE_IMAGE,
    DS_CONTENT_TYPE_VIDEO,
    DS_CONTENT_TYPE_AUDIO
} DS_ContentType;

typedef struct {
    DS_ContentType type;
    union {
        struct {
            const char *text;
        };
        struct {
            const char *url;
        } image;
        struct {
            const char **frames;
            size_t count;
        } video;
        struct {
            const char *url;
        } audio;
    };
} DS_Content;

DS_Content ds_content_text(const char *text);
DS_Content ds_content_image(const char *url);
DS_Content ds_content_video(const char **frames, size_t count);
DS_Content ds_content_audio(const char *url);

typedef enum {
    DS_MESSAGE_ROLE_USER,
    DS_MESSAGE_ROLE_ASSISTANT,
    DS_MESSAGE_ROLE_SYSTEM
} DS_MessageRole;

typedef struct {
    DS_MessageRole role;
    DS_Content *content;
    size_t content_count;
} DS_Message;

typedef enum {
    DS_PROPERTY_TYPE_STRING
} DS_PropertyType;

typedef struct {
    DS_PropertyType type;
    const char *name;
    const char *description;
} DS_Property;

typedef enum {
    DS_PARAMETER_TYPE_OBJECT
} DS_ParameterType;

typedef struct {
    DS_ParameterType type;

    DS_Property *properties;
    size_t property_count;

    const char **required_properties;
    size_t required_property_count;
} DS_Parameters;

typedef struct {
    const char *name;
    const char *description;

    DS_Parameters parameters;
} DS_Function;

typedef enum {
    DS_TOOL_TYPE_FUNCTION
} DS_ToolType;

typedef struct {
    DS_ToolType type;
    union {
        DS_Function function;
    };
} DS_Tool;

typedef struct {
    const char *name;
    const char *arguments;
} DS_ToolCall;

typedef enum {
    DS_FINISH_REASON_STOP,
    DS_FINISH_REASON_LENGTH,
    DS_FINISH_REASON_TOOL_CALL,
    DS_FINISH_REASON_NULL
} DS_FinishReason;

typedef struct {
    DS_Message *message;
    size_t message_count;

    DS_ToolCall *tool_calls;
    size_t tool_call_count;

    DS_FinishReason finish_reason;
} DS_Choice;

typedef struct {
    size_t input_tokens;
    size_t output_tokens;
    size_t total_tokens;
} DS_Usage;

typedef struct {
    DS_Choice *choices;
    size_t choice_count;

    DS_Usage usage;
} DS_Output;

typedef struct {
    const char *request_id;
    DS_Output output;
} DS_Response;

DS_Response *ds_generate(const DS_Generation *generation, const DS_Message *messages, size_t message_count, const DS_Tool *tools, size_t tool_count);
void ds_free_response(DS_Response *response);

#endif /* DASHSCOPE_H */

#ifdef DASHSCOPE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

DS_Content ds_content_text(const char *text) { return (DS_Content) { .type = DS_CONTENT_TYPE_TEXT, .text = text }; }
DS_Content ds_content_image(const char *url) { return (DS_Content) { .type = DS_CONTENT_TYPE_IMAGE, .image.url = url }; }
DS_Content ds_content_video(const char **frames, size_t count) { return (DS_Content) { .type = DS_CONTENT_TYPE_VIDEO, .video.frames = frames, .video.count = count }; }
DS_Content ds_content_audio(const char *url) { return (DS_Content) { .type = DS_CONTENT_TYPE_AUDIO, .audio.url = url }; }

static char *gen_request_data(const DS_Generation *generation, const DS_Message *messages, size_t message_count, const DS_Tool *tools, size_t tool_count) {
    char *request_data = NULL;
    cJSON *json = cJSON_CreateObject();
    /* TODO: Implement request data generation */
    request_data = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return request_data;
}

static char *call_api(const char *request_data) {
    /* TODO: Implement API call by curl */
    return NULL;
}

static DS_Response *parse_response(const char *response_data) {
    /* TODO: Implement response parsing */
    return NULL;
}

DS_Response *ds_generate(const DS_Generation *generation, const DS_Message *messages, size_t message_count, const DS_Tool *tools, size_t tool_count) {
    char *request_data = gen_request_data(generation, messages, message_count, tools, tool_count);
    char *response_data = call_api(request_data);
    free(request_data);
    DS_Response *response = parse_response(response_data);
    free(response_data);
    return response;
}

static void ds_content_free(DS_Content *content) {
    if (!content) return;
    switch (content->type) {
        case DS_CONTENT_TYPE_TEXT:
            free((void*)content->text);
            break;
        case DS_CONTENT_TYPE_IMAGE:
            free((void*)content->image.url);
            break;
        case DS_CONTENT_TYPE_VIDEO:
            if (content->video.frames) {
                for (size_t i = 0; i < content->video.count; ++i) {
                    free((void*)content->video.frames[i]);
                }
                free(content->video.frames);
            }
            break;
        case DS_CONTENT_TYPE_AUDIO:
            free((void*)content->audio.url);
            break;
    }
}

static void ds_message_free(DS_Message *msg) {
    if (!msg) return;
    if (msg->content) {
        for (size_t i = 0; i < msg->content_count; ++i) {
            ds_content_free(&msg->content[i]);
        }
        free(msg->content);
    }
}

static void ds_tool_call_free(DS_ToolCall *call) {
    if (!call) return;
    free((void*)call->name);
    free((void*)call->arguments);
}

void ds_free_response(DS_Response *resp) {
    if (!resp) return;

    if (resp->output.choices) {
        for (size_t i = 0; i < resp->output.choice_count; ++i) {
            DS_Choice *choice = &resp->output.choices[i];
            if (choice->message) {
                for (size_t j = 0; j < choice->message_count; ++j) {
                    ds_message_free(&choice->message[j]);
                }
                free(choice->message);
            }
            if (choice->tool_calls) {
                for (size_t j = 0; j < choice->tool_call_count; ++j) {
                    ds_tool_call_free(&choice->tool_calls[j]);
                }
                free(choice->tool_calls);
            }
        }
        free(resp->output.choices);
    }

    free((void*)resp->request_id);

    memset(resp, 0, sizeof(DS_Response));
}

#endif /* DASHSCOPE_IMPLEMENTATION */
