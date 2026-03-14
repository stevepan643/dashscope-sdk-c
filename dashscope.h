/*
 * DashScope SDK for C
 * 
 * A lightweight C SDK for interacting with Alibaba DashScope API.
 * Supports text generation, multimodal processing, and function calls.
 * 
 * Version: 0.1.0
 * License: Apache 2.0
 */

#ifndef DASHSCOPE_H
#define DASHSCOPE_H

#define _XOPEN_SOURCE 700

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

typedef enum {
    DS_MODEL_QWEN_PLUS,
    DS_MODEL_QWEN_VL_PLUS,
    DS_MODEL_QWEN_VL_MAX_LATEST,
    DS_MODEL_QWEN2_AUDIO_INSTRUCT,

    DS_MODEL_COUNT
} DS_Model;

typedef enum {
    DS_MODEL_CAP_TEXT        = 1 << 0,
    DS_MODEL_CAP_IMAGE       = 1 << 1,
    DS_MODEL_CAP_VIDEO       = 1 << 2,
    DS_MODEL_CAP_AUDIO       = 1 << 3,
    DS_MODEL_CAP_TOOL_CALL   = 1 << 4,
    DS_MODEL_CAP_STREAM      = 1 << 5
} DS_ModelCapability;

typedef struct {
    DS_Model model;
    const char *name;
    unsigned int capabilities;
} DS_ModelInfo;

typedef struct {
    const char *api_key;
    const DS_Model model;

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

static const DS_ModelInfo model_infos[DS_MODEL_COUNT] = {
    { DS_MODEL_QWEN_PLUS, "qwen-plus", DS_MODEL_CAP_TEXT | DS_MODEL_CAP_TOOL_CALL },
    { DS_MODEL_QWEN_VL_PLUS, "qwen-vl-plus", DS_MODEL_CAP_TEXT | DS_MODEL_CAP_IMAGE | DS_MODEL_CAP_VIDEO | DS_MODEL_CAP_AUDIO | DS_MODEL_CAP_TOOL_CALL },
    { DS_MODEL_QWEN_VL_MAX_LATEST, "qwen-vl-max-latest", DS_MODEL_CAP_TEXT | DS_MODEL_CAP_IMAGE | DS_MODEL_CAP_VIDEO | DS_MODEL_CAP_AUDIO | DS_MODEL_CAP_TOOL_CALL | DS_MODEL_CAP_STREAM },
    { DS_MODEL_QWEN2_AUDIO_INSTRUCT, "qwen2-audio-instruct", DS_MODEL_CAP_TEXT | DS_MODEL_CAP_AUDIO | DS_MODEL_CAP_TOOL_CALL }
};

static inline const DS_ModelInfo *ds_model_info(DS_Model model)
{
    if (model >= DS_MODEL_COUNT)
        return NULL;

    return &model_infos[model];
}

static inline bool ds_model_is_multimodal(DS_Model model)
{
    const DS_ModelInfo *info = ds_model_info(model);

    if (!info) return false;

    return info->capabilities &
        (DS_MODEL_CAP_IMAGE |
         DS_MODEL_CAP_VIDEO |
         DS_MODEL_CAP_AUDIO);
}

DS_Content ds_content_text(const char *text) { return (DS_Content) { .type = DS_CONTENT_TYPE_TEXT, .text = text }; }
DS_Content ds_content_image(const char *url) { return (DS_Content) { .type = DS_CONTENT_TYPE_IMAGE, .image.url = url }; }
DS_Content ds_content_video(const char **frames, size_t count) { return (DS_Content) { .type = DS_CONTENT_TYPE_VIDEO, .video.frames = frames, .video.count = count }; }
DS_Content ds_content_audio(const char *url) { return (DS_Content) { .type = DS_CONTENT_TYPE_AUDIO, .audio.url = url }; }

static char *gen_request_data(const DS_Generation *generation, const DS_Message *messages, size_t message_count, const DS_Tool *tools, size_t tool_count) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    const DS_ModelInfo *model_info = ds_model_info(generation->model);
    if (!model_info) return NULL;

    cJSON_AddStringToObject(root, "model", model_info->name);

    cJSON *input = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "input", input);

    cJSON *messages_json = cJSON_CreateArray();
    cJSON_AddItemToObject(input, "messages", messages_json);

    for (size_t i = 0; i < message_count; ++i) {
        const DS_Message *msg = &messages[i];

        cJSON *message = cJSON_CreateObject();
        cJSON_AddItemToArray(messages_json, message);

        const char *role =
            msg->role == DS_MESSAGE_ROLE_USER ? "user" :
            msg->role == DS_MESSAGE_ROLE_ASSISTANT ? "assistant" :
            "system";

        cJSON_AddStringToObject(message, "role", role);

        /*
         * Single text content: add directly as string
         */
        if (msg->content_count == 1 &&
            msg->content[0].type == DS_CONTENT_TYPE_TEXT) {

            cJSON_AddStringToObject(
                message,
                "content",
                msg->content[0].text
            );
            continue;
        }

        /*
         * Multimodal content: add as array
         */
        cJSON *content = cJSON_CreateArray();
        cJSON_AddItemToObject(message, "content", content);

        for (size_t j = 0; j < msg->content_count; ++j) {

            const DS_Content *cnt = &msg->content[j];
            cJSON *item = cJSON_CreateObject();
            cJSON_AddItemToArray(content, item);

            switch (cnt->type) {

                case DS_CONTENT_TYPE_TEXT:
                    cJSON_AddStringToObject(item,"text",cnt->text);
                    break;

                case DS_CONTENT_TYPE_IMAGE:
                    cJSON_AddStringToObject(item,"image",cnt->image.url);
                    break;

                case DS_CONTENT_TYPE_AUDIO:
                    cJSON_AddStringToObject(item,"audio",cnt->audio.url);
                    break;

                case DS_CONTENT_TYPE_VIDEO: {
                    cJSON *video = cJSON_CreateArray();
                    cJSON_AddItemToObject(item,"video",video);

                    for (size_t k = 0; k < cnt->video.count; ++k) {
                        cJSON_AddItemToArray(
                            video,
                            cJSON_CreateString(cnt->video.frames[k])
                        );
                    }
                    break;
                }
            }
        }
    }

    /* ---------- parameters ---------- */

    cJSON *parameters = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "parameters", parameters);
    cJSON_AddStringToObject(parameters, "result_format", "message");

    /*
     * Build tool definitions if provided
     */
    if (tool_count > 0) {

        cJSON *tools_json = cJSON_CreateArray();
        cJSON_AddItemToObject(parameters, "tools", tools_json);

        for (size_t i = 0; i < tool_count; ++i) {

            const DS_Tool *tl = &tools[i];

            cJSON *tool = cJSON_CreateObject();
            cJSON_AddItemToArray(tools_json, tool);

            cJSON_AddStringToObject(tool,"type","function");

            cJSON *function = cJSON_CreateObject();
            cJSON_AddItemToObject(tool,"function",function);

            cJSON_AddStringToObject(
                function,
                "name",
                tl->function.name
            );

            cJSON_AddStringToObject(
                function,
                "description",
                tl->function.description
            );

            cJSON *params = cJSON_CreateObject();
            cJSON_AddItemToObject(function,"parameters",params);

            cJSON_AddStringToObject(params,"type","object");

            cJSON *properties = cJSON_CreateObject();
            cJSON_AddItemToObject(params,"properties",properties);

            for (size_t j = 0;
                 j < tl->function.parameters.property_count;
                 ++j)
            {
                const DS_Property *prop =
                    &tl->function.parameters.properties[j];

                cJSON *prop_obj = cJSON_CreateObject();
                cJSON_AddItemToObject(properties,prop->name,prop_obj);

                cJSON_AddStringToObject(prop_obj,"type","string");

                if (prop->description)
                    cJSON_AddStringToObject(
                        prop_obj,
                        "description",
                        prop->description
                    );
            }

            if (tl->function.parameters.required_property_count) {

                cJSON *required = cJSON_CreateArray();
                cJSON_AddItemToObject(params,"required",required);

                for (size_t j = 0;
                     j < tl->function.parameters.required_property_count;
                     ++j)
                {
                    cJSON_AddItemToArray(
                        required,
                        cJSON_CreateString(
                            tl->function.parameters.required_properties[j]
                        )
                    );
                }
            }
        }
    }

    char *result = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return result;
}

struct CurlBuffer {
    char *data;
    size_t size;
};

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t real_size = size * nmemb;
    struct CurlBuffer *buf = (struct CurlBuffer*)userdata;

    char *new_data = (char*)realloc(buf->data, buf->size + real_size + 1);
    if (!new_data) return 0;

    buf->data = new_data;
    memcpy(buf->data + buf->size, ptr, real_size);
    buf->size += real_size;
    buf->data[buf->size] = '\0';

    return real_size;
}

static char *call_api(const DS_Generation *generation, const char *request_data) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize CURL.\n");
        return NULL;
    }

    struct CurlBuffer buf = {0};
    buf.data = (char*)malloc(1);
    buf.size = 0;

    struct curl_slist *headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", generation->api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (!ds_model_is_multimodal(generation->model)) { 
        curl_easy_setopt(curl, CURLOPT_URL, "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation");
    } else { 
        curl_easy_setopt(curl, CURLOPT_URL, "https://dashscope.aliyuncs.com/api/v1/services/aigc/multimodal-generation/generation"); 
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_data);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&buf);

    /*
     * Perform HTTP POST request
     */
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
        free(buf.data);
        buf.data = NULL;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    /*
     * Return response string (caller must free)
     */
    return buf.data;
}

static DS_Response *parse_response(const char *response_data) {
    if (!response_data) return NULL;

    cJSON *root = cJSON_Parse(response_data);
    if (!root) {
        fprintf(stderr, "Failed to parse JSON response\n");
        return NULL;
    }

    DS_Response *response = (DS_Response*)malloc(sizeof(DS_Response));
    if (!response) {
        cJSON_Delete(root);
        return NULL;
    }

    memset(response, 0, sizeof(DS_Response));

    /* Parse request_id (can be at root or in output) */
    cJSON *request_id_item = cJSON_GetObjectItem(root, "request_id");
    if (!request_id_item) {
        cJSON *output = cJSON_GetObjectItem(root, "output");
        if (output) {
            request_id_item = cJSON_GetObjectItem(output, "request_id");
        }
    }
    if (request_id_item && request_id_item->valuestring) {
        response->request_id = strdup(request_id_item->valuestring);
    }

    /* Parse usage (can be at root or in output) */
    cJSON *usage = cJSON_GetObjectItem(root, "usage");
    if (!usage) {
        cJSON *output = cJSON_GetObjectItem(root, "output");
        if (output) {
            usage = cJSON_GetObjectItem(output, "usage");
        }
    }
    if (usage) {
        cJSON *input_tokens = cJSON_GetObjectItem(usage, "input_tokens");
        cJSON *output_tokens = cJSON_GetObjectItem(usage, "output_tokens");
        cJSON *total_tokens = cJSON_GetObjectItem(usage, "total_tokens");

        if (input_tokens && input_tokens->type == cJSON_Number) {
            response->output.usage.input_tokens = (size_t)input_tokens->valuedouble;
        }
        if (output_tokens && output_tokens->type == cJSON_Number) {
            response->output.usage.output_tokens = (size_t)output_tokens->valuedouble;
        }
        if (total_tokens && total_tokens->type == cJSON_Number) {
            response->output.usage.total_tokens = (size_t)total_tokens->valuedouble;
        }
    }

    /* Parse output */
    cJSON *output = cJSON_GetObjectItem(root, "output");
    if (!output) {
        cJSON_Delete(root);
        return response;
    }

    /* Parse choices */
    cJSON *choices = cJSON_GetObjectItem(output, "choices");
    if (!choices || choices->type != cJSON_Array) {
        cJSON_Delete(root);
        return response;
    }

    response->output.choice_count = (size_t)cJSON_GetArraySize(choices);
    if (response->output.choice_count > 0) {
        response->output.choices = (DS_Choice*)calloc(response->output.choice_count, sizeof(DS_Choice));
        if (!response->output.choices) {
            response->output.choice_count = 0;
            cJSON_Delete(root);
            return response;
        }
    }

    size_t choice_idx = 0;
    cJSON *choice_item = NULL;
    cJSON_ArrayForEach(choice_item, choices) {
        if (choice_idx >= response->output.choice_count) break;

        DS_Choice *choice = &response->output.choices[choice_idx];

        /* Parse finish_reason */
        cJSON *finish_reason_item = cJSON_GetObjectItem(choice_item, "finish_reason");
        if (finish_reason_item && finish_reason_item->valuestring) {
            const char *reason = finish_reason_item->valuestring;
            if (strcmp(reason, "stop") == 0) {
                choice->finish_reason = DS_FINISH_REASON_STOP;
            } else if (strcmp(reason, "length") == 0) {
                choice->finish_reason = DS_FINISH_REASON_LENGTH;
            } else if (strcmp(reason, "tool_calls") == 0) {
                choice->finish_reason = DS_FINISH_REASON_TOOL_CALL;
            } else {
                choice->finish_reason = DS_FINISH_REASON_NULL;
            }
        }

        /* Parse message */
        cJSON *message_item = cJSON_GetObjectItem(choice_item, "message");
        if (message_item && message_item->type == cJSON_Object) {
            choice->message_count = 1;
            choice->message = (DS_Message*)malloc(sizeof(DS_Message));
            if (choice->message) {
                memset(choice->message, 0, sizeof(DS_Message));

                /* Parse message role */
                cJSON *role_item = cJSON_GetObjectItem(message_item, "role");
                if (role_item && role_item->valuestring) {
                    const char *role = role_item->valuestring;
                    if (strcmp(role, "user") == 0) {
                        choice->message->role = DS_MESSAGE_ROLE_USER;
                    } else if (strcmp(role, "assistant") == 0) {
                        choice->message->role = DS_MESSAGE_ROLE_ASSISTANT;
                    } else if (strcmp(role, "system") == 0) {
                        choice->message->role = DS_MESSAGE_ROLE_SYSTEM;
                    }
                }

                /* Parse message content */
                cJSON *content_item = cJSON_GetObjectItem(message_item, "content");
                if (content_item) {
                    if (content_item->type == cJSON_String) {
                        /* Single text content */
                        choice->message->content_count = 1;
                        choice->message->content = (DS_Content*)malloc(sizeof(DS_Content));
                        if (choice->message->content) {
                            memset(choice->message->content, 0, sizeof(DS_Content));
                            choice->message->content->type = DS_CONTENT_TYPE_TEXT;
                            choice->message->content->text = strdup(content_item->valuestring);
                        }
                    } else if (content_item->type == cJSON_Array) {
                        /* Multiple content items (multimodal) */
                        choice->message->content_count = (size_t)cJSON_GetArraySize(content_item);
                        if (choice->message->content_count > 0) {
                            choice->message->content = (DS_Content*)calloc(choice->message->content_count, sizeof(DS_Content));
                            if (choice->message->content) {
                                size_t content_idx = 0;
                                cJSON *content_obj = NULL;
                                cJSON_ArrayForEach(content_obj, content_item) {
                                    if (content_idx >= choice->message->content_count) break;

                                    DS_Content *cnt = &choice->message->content[content_idx];
                                    memset(cnt, 0, sizeof(DS_Content));

                                    cJSON *text_item = cJSON_GetObjectItem(content_obj, "text");
                                    if (text_item && text_item->valuestring) {
                                        cnt->type = DS_CONTENT_TYPE_TEXT;
                                        cnt->text = strdup(text_item->valuestring);
                                    }

                                    content_idx++;
                                }
                            }
                        }
                    }
                }
            }
        }

        /* Parse tool_calls (can be in message or directly in choice) */
        cJSON *tool_calls_item = cJSON_GetObjectItem(message_item, "tool_calls");
        if (!tool_calls_item) {
            tool_calls_item = cJSON_GetObjectItem(choice_item, "tool_calls");
        }
        
        if (tool_calls_item && tool_calls_item->type == cJSON_Array) {
            choice->tool_call_count = (size_t)cJSON_GetArraySize(tool_calls_item);
            if (choice->tool_call_count > 0) {
                choice->tool_calls = (DS_ToolCall*)calloc(choice->tool_call_count, sizeof(DS_ToolCall));
                if (choice->tool_calls) {
                    size_t call_idx = 0;
                    cJSON *call_item = NULL;
                    cJSON_ArrayForEach(call_item, tool_calls_item) {
                        if (call_idx >= choice->tool_call_count) break;

                        DS_ToolCall *call = &choice->tool_calls[call_idx];

                        cJSON *name_item = cJSON_GetObjectItem(call_item, "function");
                        if (name_item) {
                            /* Try to get name from nested function.name or direct name */
                            cJSON *fn_name = cJSON_GetObjectItem(name_item, "name");
                            if (fn_name && fn_name->valuestring) {
                                call->name = strdup(fn_name->valuestring);
                            }

                            /* Try to get arguments from function.arguments or direct arguments */
                            cJSON *fn_args = cJSON_GetObjectItem(name_item, "arguments");
                            if (fn_args) {
                                if (fn_args->type == cJSON_String) {
                                    call->arguments = strdup(fn_args->valuestring);
                                } else {
                                    char *args_str = cJSON_PrintUnformatted(fn_args);
                                    if (args_str) {
                                        call->arguments = args_str;
                                    }
                                }
                            }
                        } else {
                            /* Try direct name and arguments fields */
                            cJSON *direct_name = cJSON_GetObjectItem(call_item, "name");
                            cJSON *direct_args = cJSON_GetObjectItem(call_item, "arguments");

                            if (direct_name && direct_name->valuestring) {
                                call->name = strdup(direct_name->valuestring);
                            }
                            if (direct_args) {
                                if (direct_args->type == cJSON_String) {
                                    call->arguments = strdup(direct_args->valuestring);
                                } else {
                                    char *args_str = cJSON_PrintUnformatted(direct_args);
                                    if (args_str) {
                                        call->arguments = args_str;
                                    }
                                }
                            }
                        }

                        call_idx++;
                    }
                }
            }
        }

        choice_idx++;
    }

    cJSON_Delete(root);
    return response;
}

DS_Response *ds_generate(const DS_Generation *generation, const DS_Message *messages, size_t message_count, const DS_Tool *tools, size_t tool_count) {
    char *request_data = gen_request_data(generation, messages, message_count, tools, tool_count);
    printf("Request Data:\n%s\n", request_data);
    char *response_data = call_api(generation, request_data);
    free(request_data);
    DS_Response *response = parse_response(response_data);
    printf("Response Data:\n%s\n", response_data);
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

/*
 * Version History:
 * 
 * v0.1.0 (2026-03-14)
 *   - Initial release with support for:
 *     * Text generation with qwen-plus model
 *     * Multimodal input (images, videos, audio)
 *     * Function calling / tool use
 *     * Full JSON request/response parsing
 *     * Memory management with cleanup helpers
 *   - Supported models:
 *     * DS_MODEL_QWEN_PLUS
 *     * DS_MODEL_QWEN_VL_PLUS
 *     * DS_MODEL_QWEN_VL_MAX_LATEST
 *     * DS_MODEL_QWEN2_AUDIO_INSTRUCT
 *   - Features:
 *     * Async HTTP via libcurl
 *     * JSON processing via cJSON
 *     * No external state management
 */
