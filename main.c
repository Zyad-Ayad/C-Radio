#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include "cJSON.h"
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


enum ip_version {
    IPV4,
    IPV6
};

struct server {
    char *ip;
    char *hostname;
    enum ip_version ip_version;
    struct server* next;
};

struct country {
    char *country_code;
    char *country_name;
    int station_count;
    struct country* next;
};

//simple data required for minimum station info (name and url)
struct station {
    char *name;
    char *url;
    struct station* next;
};

struct Memory {
    char *response;
    size_t size;
};

#define HOST "all.api.radio-browser.info"

struct server* dns_lookup(const char *hostname) {
    struct addrinfo hints, *res, *p;
    char ipstr[INET6_ADDRSTRLEN];

    // Zero out the hints structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    // Get address information
    int status = getaddrinfo(hostname, NULL, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return NULL;
    }

    // Loop through all results and convert them to string format
    int servers_count = 0;
    for (p = res; p != NULL; p = p->ai_next) {
        servers_count++;
    }

    struct server* root = NULL;
    struct server* now = NULL;
    for (p = res; p != NULL; p = p->ai_next) {


        if(root==NULL)
        {
            root = (struct server*)malloc(sizeof(struct server));
            now = root;
        }
        else{
            now->next = (struct server*)malloc(sizeof(struct server));
            now = now->next;
        }

        memset(now, 0 ,sizeof(now));
        
        // Check whether it's IPv4 or IPv6
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            now->ip = malloc(INET_ADDRSTRLEN);
            inet_ntop(p->ai_family, &(ipv4->sin_addr), now->ip, INET_ADDRSTRLEN);
            now->ip_version = IPV4;
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            now->ip = malloc(INET6_ADDRSTRLEN);
            inet_ntop(p->ai_family, &(ipv6->sin6_addr), now->ip, INET6_ADDRSTRLEN);
            now->ip_version = IPV6;
        }

        char host[NI_MAXHOST];
        
        if(now->ip_version == IPV4)
        {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            inet_pton(AF_INET, now->ip, &(addr.sin_addr));
            getnameinfo((struct sockaddr*)&addr, sizeof(addr), host, sizeof(host), NULL, 0, 0);
        }
        else
        {
            struct sockaddr_in6 addr;
            addr.sin6_family = AF_INET6;
            inet_pton(AF_INET6, now->ip, &(addr.sin6_addr));
            getnameinfo((struct sockaddr*)&addr, sizeof(addr), host, sizeof(host), NULL, 0, 0);
        }

        now->hostname = malloc(strlen(host)+1);
        strcpy(now->hostname, host);
        now->next = NULL;
    }

    freeaddrinfo(res);
    return root;
}



size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
    size_t realsize = size * nmemb;
    struct Memory *mem = (struct Memory *)data;

    char *ptr2 = realloc(mem->response, mem->size + realsize + 1);
    if (ptr2 == NULL) {
        return 0;
    }

    mem->response = ptr2;
    memcpy(&(mem->response[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}


//fetch countries

struct country* fetch_countries(char* hostname){

    //debug
    // printf("fetching countries from %s\n", hostname);

    char* path = "/json/countries";
    char* url = malloc(strlen(hostname)+strlen(path)+1);
    strcpy(url, hostname);
    strcat(url, path);

    CURL *curl;
    CURLcode res;

    struct Memory chunk;
    chunk.response = malloc(1);
    chunk.size = 0;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(url);
            free(chunk.response);
            return NULL;
        }
        curl_easy_cleanup(curl);
    }

    //debug
    //  printf("response: %s\n", chunk.response);
    cJSON *json = cJSON_Parse(chunk.response);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        free(url);
        free(chunk.response);
        return NULL;
    }
    


    struct country* root = NULL;
    struct country* now = NULL;
    cJSON *country_loop = NULL;

    cJSON_ArrayForEach(country_loop, json) {
        if(root==NULL)
        {
            root = (struct country*)malloc(sizeof(struct country));
            now = root;
        }
        else{
            now->next = (struct country*)malloc(sizeof(struct country));
            now = now->next;
        }
        memset(now, 0 ,sizeof(now));
        

        cJSON *country_code = cJSON_GetObjectItem(country_loop, "iso_3166_1");
        cJSON *country_name = cJSON_GetObjectItem(country_loop, "name");
        cJSON *station_count = cJSON_GetObjectItem(country_loop, "stationcount");

        now->country_code = malloc(strlen(country_code->valuestring)+1);
        strcpy(now->country_code, country_code->valuestring);
        now->country_name = malloc(strlen(country_name->valuestring)+1);
        strcpy(now->country_name, country_name->valuestring);
        now->station_count = station_count->valueint;
        now->next = NULL;

    }

    cJSON_Delete(json);
    free(chunk.response);
    free(url);
    return root;
}


struct station* fetch_startions(char* hostname, char* country){

    char* path = "/json/stations/bycountry/";
    char* url = malloc(strlen(hostname)+strlen(path)+strlen(country)+1);
    strcpy(url, hostname);
    strcat(url, path);
    strcat(url, country);

    CURL *curl;
    CURLcode res;

    struct Memory chunk;
    chunk.response = malloc(1);
    chunk.size = 0;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(url);
            free(chunk.response);
            return NULL;

        }
        curl_easy_cleanup(curl);
    }


    cJSON *json = cJSON_Parse(chunk.response);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        free(url);
        free(chunk.response);
        return NULL;
    }

    struct station* root = NULL;
    struct station* now = NULL;
    cJSON *station_loop = NULL;

    cJSON_ArrayForEach(station_loop, json) {
        if(root==NULL)
        {
            root = (struct station*)malloc(sizeof(struct station));
            now = root;
        }
        else{
            now->next = (struct station*)malloc(sizeof(struct station));
            now = now->next;
        }
        memset(now, 0 ,sizeof(now));
        

        cJSON *station_name = cJSON_GetObjectItem(station_loop, "name");
        cJSON *station_url = cJSON_GetObjectItem(station_loop, "url");

        now->name = malloc(strlen(station_name->valuestring)+1);
        strcpy(now->name, station_name->valuestring);
        now->url = malloc(strlen(station_url->valuestring)+1);
        strcpy(now->url, station_url->valuestring);
        now->next = NULL;

    }

    cJSON_Delete(json);
    free(chunk.response);
    free(url);
    return root;


}


int play_stream(const char *url) {
    int player_pid = fork();
    if (player_pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (player_pid == 0) { 
        if (setpgid(0, 0) != 0) {
            perror("setpgid failed");
            exit(EXIT_FAILURE);
        }
        execlp("sh", "sh", "-c", "curl -s \"$0\" | ffplay -nodisp -autoexit -loglevel quiet -", url, (char *)NULL);
        perror("execlp failed");
        exit(EXIT_FAILURE);
    }
    return player_pid;
}

void stop_playback(int player_pid) {
    printf("\nStopping playback...\n");

    if (killpg(player_pid, SIGTERM) != 0) {
        perror("killpg(SIGTERM) failed");
    }
    sleep(1);  

    int status;
    pid_t result = waitpid(player_pid, &status, WNOHANG);
    if (result < 0) {
        perror("waitpid failed");
    } else if (result == 0) {
        // kill the damn process
        printf("Process did not exit, forcing termination...\n");
        if (killpg(player_pid, SIGKILL) != 0) {
            perror("killpg(SIGKILL) failed");
        }
    }

    if (waitpid(player_pid, &status, 0) < 0) {
        perror("waitpid during cleanup failed");
    }
    printf("Playback stopped.\n");
}



int main(int argc, char *argv[]) {

    struct server* servers = dns_lookup(HOST);
    if (servers == NULL) {
        fprintf(stderr, "Error in dns lookup or maybe no servers available\n");
        return EXIT_FAILURE;
    }
    
    int servers_count = 0;
    struct server* temp = servers;
    while (temp != NULL) {
        servers_count++;
        temp = temp->next;
    }
    struct server* servers_array = malloc(sizeof(struct server) * servers_count);
    if (!servers_array) {
        perror("malloc failed");
        return EXIT_FAILURE;
    }
    temp = servers;
    for (int i = 0; i < servers_count; i++) {
        servers_array[i].ip = malloc(strlen(temp->ip) + 1);
        strcpy(servers_array[i].ip, temp->ip);
        servers_array[i].hostname = malloc(strlen(temp->hostname) + 1);
        strcpy(servers_array[i].hostname, temp->hostname);
        servers_array[i].ip_version = temp->ip_version;
        temp = temp->next;
    }

    while (servers != NULL) {
        struct server* next = servers->next;
        free(servers->ip);
        free(servers->hostname);
        free(servers);
        servers = next;
    }

    while (1) {

        int chosen_server = -1;
        printf("Choose a server to connect to:\n");
        for (int i = 0; i < servers_count; i++) {
            printf("%d. %s (%s)\n", i + 1, servers_array[i].hostname, servers_array[i].ip);
        }
        printf("Enter the server number: ");
        while (scanf("%d", &chosen_server) != 1 || chosen_server < 1 || chosen_server > servers_count) {
            printf("Invalid server number. Please enter a valid server number: ");
            while (getchar() != '\n');  
        }
        chosen_server--;

        printf("Connecting to %s (%s)...\n", servers_array[chosen_server].hostname, servers_array[chosen_server].ip);

        struct country* countries = fetch_countries(servers_array[chosen_server].hostname);
        if (countries == NULL) {
            fprintf(stderr, "Error in fetching countries\n");
            continue;  
        }

        int countries_count = 0;
        struct country* temp_country = countries;
        while (temp_country != NULL) {
            countries_count++;
            temp_country = temp_country->next;
        }
        struct country* countries_array = malloc(sizeof(struct country) * countries_count);
        if (!countries_array) {
            perror("malloc failed");
            continue;
        }
        temp_country = countries;
        for (int i = 0; i < countries_count; i++) {
            countries_array[i].country_code = malloc(strlen(temp_country->country_code) + 1);
            strcpy(countries_array[i].country_code, temp_country->country_code);
            countries_array[i].country_name = malloc(strlen(temp_country->country_name) + 1);
            strcpy(countries_array[i].country_name, temp_country->country_name);
            countries_array[i].station_count = temp_country->station_count;
            temp_country = temp_country->next;
        }
        while (countries != NULL) {
            struct country* next = countries->next;
            free(countries->country_code);
            free(countries->country_name);
            free(countries);
            countries = next;
        }

        while (1) {
            int chosen_country = -1;
            printf("Choose a country to view stations:\n");
            for (int i = 0; i < countries_count; i++) {
                printf("%d. %s (%s) - %d stations\n", i + 1, countries_array[i].country_name,
                       countries_array[i].country_code, countries_array[i].station_count);
            }
            printf("Enter the country number or 0 to go back: ");
            while (scanf("%d", &chosen_country) != 1 || chosen_country < 0 || chosen_country > countries_count) {
                printf("Invalid country number. Please enter a valid country number: ");
                while (getchar() != '\n');
            }
            if (chosen_country == 0) {
                break; 
            }
            chosen_country--;

            printf("Fetching stations for %s (%s)...\n", countries_array[chosen_country].country_name,
                   countries_array[chosen_country].country_code);

            struct station* stations = fetch_startions(servers_array[chosen_server].hostname,
                                                        countries_array[chosen_country].country_code);
            if (stations == NULL) {
                fprintf(stderr, "Error in fetching stations\n");
                continue; 
            }

            int stations_count = 0;
            struct station* temp_station = stations;
            while (temp_station != NULL) {
                stations_count++;
                temp_station = temp_station->next;
            }
            struct station* stations_array = malloc(sizeof(struct station) * stations_count);
            if (!stations_array) {
                perror("malloc failed");
                continue;
            }
            temp_station = stations;
            for (int i = 0; i < stations_count; i++) {
                stations_array[i].name = malloc(strlen(temp_station->name) + 1);
                strcpy(stations_array[i].name, temp_station->name);
                stations_array[i].url = malloc(strlen(temp_station->url) + 1);
                strcpy(stations_array[i].url, temp_station->url);
                temp_station = temp_station->next;
            }
            while (stations != NULL) {
                struct station* next = stations->next;
                free(stations->name);
                free(stations->url);
                free(stations);
                stations = next;
            }

            while (1) {
                int chosen_station = -1;
                printf("Choose a station to play:\n");
                for (int i = 0; i < stations_count; i++) {
                    printf("%d. %s\n", i + 1, stations_array[i].name);
                }
                printf("Enter the station number or 0 to go back: ");
                while (scanf("%d", &chosen_station) != 1 || chosen_station < 0 || chosen_station > stations_count) {
                    printf("Invalid station number. Please enter a valid station number: ");
                    while (getchar() != '\n');
                }
                if (chosen_station == 0) {
                    break; 
                }
                chosen_station--;

                printf("Playing %s...\n", stations_array[chosen_station].name);
                int player_id = play_stream(stations_array[chosen_station].url);
                if (player_id < 0) {
                    fprintf(stderr, "Error starting playback\n");
                    continue;
                }

                printf("Enter 0 to stop playback: ");
                int input = -1;
                while (scanf("%d", &input) != 1 || input != 0) {
                    printf("Invalid input. Please enter 0 to stop playback: ");
                    while (getchar() != '\n');
                }
                stop_playback(player_id);
            }

            // Free station array memory
            for (int i = 0; i < stations_count; i++) {
                free(stations_array[i].name);
                free(stations_array[i].url);
            }
            free(stations_array);
        }
        // Free country array memory
        for (int i = 0; i < countries_count; i++) {
            free(countries_array[i].country_code);
            free(countries_array[i].country_name);
        }
        free(countries_array);
    }

    for (int i = 0; i < servers_count; i++) {
        free(servers_array[i].ip);
        free(servers_array[i].hostname);
    }
    free(servers_array);

    return EXIT_SUCCESS;
}
