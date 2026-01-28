#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

struct curl_response { char *html; size_t size; };

static size_t write_html_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct curl_response *mem = (struct curl_response *)userp;
    char *ptr = realloc(mem->html, mem->size + realsize + 1);

    if (!ptr) return 0;

    mem->html = ptr;
    memcpy(&(mem->html[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->html[mem->size] = 0;

    return realsize;
}

struct curl_response get_html(const char *url)
{
    CURL *curl = curl_easy_init();
    struct curl_response res = {.html = malloc(1), .size = 0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_html_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&res);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return res;
}

htmlDocPtr parse_html(const char *html, size_t size)
{
    return htmlReadMemory(
        html,
        (int)size,
        NULL,
        NULL,
        HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING
    );
}

static int is_space(char c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f';
}

static int is_block_element(const xmlChar *name)
{
    static const char *blocks[] = {
        "p","div","br","section","article","header","footer",
        "li","ul","ol","table","tr","td","th",
        "h1","h2","h3","h4","h5","h6",
        "pre","blockquote", NULL
    };

    for (int i = 0; blocks[i]; i++)
        if (!xmlStrcasecmp(name, (const xmlChar *)blocks[i]))
            return 1;

    return 0;
}

static void print_normalized(const xmlChar *s)
{
    int in_space = 0;

    for (; *s; s++) {
        if (is_space(*s)) {
            in_space = 1;
        } else {
            if (in_space) {
                putchar(' ');
                in_space = 0;
            }
            putchar(*s);
        }
    }
}

void print_plain_text(htmlDocPtr doc)
{
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) return;

    xmlXPathObjectPtr obj =
        xmlXPathEvalExpression(
            (const xmlChar *)"//text()[normalize-space()]",
            ctx
        );

    if (!obj || obj->type != XPATH_NODESET) {
        xmlXPathFreeContext(ctx);
        return;
    }

    xmlNodeSetPtr nodes = obj->nodesetval;
    xmlNodePtr last_block = NULL;

    for (int i = 0; i < nodes->nodeNr; i++) {
        xmlNodePtr n = nodes->nodeTab[i];
        xmlNodePtr p = n->parent;

        if (!n->content || !p) continue;

        /* Skip script/style/noscript */
        if (!xmlStrcasecmp(p->name, (xmlChar *)"script") ||
            !xmlStrcasecmp(p->name, (xmlChar *)"style") ||
            !xmlStrcasecmp(p->name, (xmlChar *)"noscript"))
            continue;

        /* Newline when entering a new block */
        if (is_block_element(p->name) && p != last_block) {
            putchar('\n');
            last_block = p;
        }

        print_normalized(n->content);
    }

    putchar('\n');

    xmlXPathFreeObject(obj);
    xmlXPathFreeContext(ctx);
}

int main(int argc, char *argv[])
{
    if (argc != 2) return 1;

    struct curl_response res = get_html(argv[1]);
    htmlDocPtr doc = parse_html(res.html, res.size);

    print_plain_text(doc);

    xmlFreeDoc(doc);
    free(res.html);
    xmlCleanupParser();

    return 0;
}
