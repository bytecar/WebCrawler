/*
 * Kartik Vedalaveni
 * March 2011
 * Communication Networks Assignment
 *
 *
 */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<iostream>
#include<fstream>
#include<string>
#include<stdlib.h>
#include<cstring>
#include<queue>
#include<time.h>
#include<map>

using namespace std;

#define USERAGENT "Crawler"
queue<string> Q;

int hrefs(string str, int i, string *ptr) {
    int l2, l1;

    if ((i = str.find("href", i)) != string::npos) {

        if ((i = str.find("\"", i)) != string::npos) {
            l1 = i + 1;
            i++;
            if ((i = str.find("\"", i)) != string::npos) {
                l2 = i;
                //cout<<"\nL1: "<<l1<<"\nL2: "<<l2<<"\nSubstring: "<<str.substr(l1,(l2-l1))<<"\n";
            } else return i;
        } else return i;
    } else return i;

    //cout << "Href: " << str.substr(l1, (l2 - l1)) << "\n";

    *ptr = str.substr(l1, (l2 - l1));

    return i;
}

char *build_http_query(char *host, char *page) {
    char *query;
    char *pagefile = page;
    //somehow http/1.1 queries perform faster
    char tpl[] = "GET /%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\n\r\n";
    //char tpl[] = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\n\r\n";

    if (pagefile[0] == '/') {
        pagefile = pagefile + 1;
        //fprintf(stderr, "Removing leading \"/\", converting %s to %s\n", page, pagefile);
    }
    // -5 is to consider the %s %s %s in tpl and the ending \0
    query = (char *) malloc(strlen(host) + strlen(pagefile) + strlen(USERAGENT) + strlen(tpl) - 5);
    sprintf(query, tpl, pagefile, host, USERAGENT);

    return query;
}

int send_http_query(int socketfd, char *get, int status) {

    int sent = 0;
    while (sent < strlen(get)) {
        status = send(socketfd, get + sent, strlen(get) - sent, 0);
        if (status == -1) {
            perror("Can't send query");
            return (3);
        }
        sent += status;
    }
}

string receive_html(int socketfd, char *buffer, int status, string *last_modified) {
    memset(buffer, 0, sizeof (buffer));

    int htmlstart = 0;
    char *htmlcontent, *last_modif;
    string str, last_mod;

    while ((status = recv(socketfd, buffer, BUFSIZ, 0)) > 0) {
        if (htmlstart == 0) {

            last_modif = strstr(buffer, "Last-Modified:");

            if (strstr(buffer, "404") != NULL)
                return "";

            if (last_modif != NULL) {
                last_modif += 19;

                for (int i = 0; i <= 20; i++) {
                    if (last_modif[i] != ',')
                        last_mod += last_modif[i];
                }
            }
            htmlcontent = strstr(buffer, "\r\n\r\n");
            if (htmlcontent != NULL) {
                htmlstart = 1;
                htmlcontent += 4;
            }
        } else {
            htmlcontent = buffer;
        }
        if (htmlstart) {
            str += htmlcontent;
        }

        memset(buffer, 0, status);
    }
    if (status < 0) {
        perror("Error receiving data");
    }

    (*last_modified) = last_mod;
    return str;
}

void parseurl(string url, string *host, string *page, string *path) {

    int pos = 0, l1, l2, l3, size = url.size();

    if ((pos = url.find("http")) != string::npos) {
        pos += 7;
        l1 = pos;
    } else {
        pos = 0;
    }

    if ((pos = url.find("/", pos)) != string::npos) {
        l2 = pos;
        *host = url.substr(l1, (l2 - l1));

        *page = url.substr(l2, size - l2);

        if ((pos = url.rfind("/")) != string::npos) {
            l3 = pos;
            *path = url.substr(l2, (l3 - l2));

            /*
            while((pos=(*page).find(" ",pos)) != string::npos) {
             (*page).replace(pos,1,"%20");
         }

        while((pos=(*page).find("~",pos)) != string::npos) {
             (*page).replace(pos,1,"%7e");
         }
             */
        }
    } else {
        *host = url;
        *page = "/";
    }

}

long long time_seconds(string date) {

    struct tm tm;
    time_t t;
    const char *datetime = date.c_str();
    datetime += 1;
    if (strptime(datetime, "%d %b %Y %H:%M:%S", &tm) == NULL)
        //if (strptime("27 Jan 2011 21:41:28", "%d %b %Y %H:%M:%S", &tm) == NULL)
        cout << "Error in strptime\n";

    //tm.tm_year=tm.tm_year;
    //tm.tm_isdst = -1; //include daylight savings
    t = mktime(&tm);

    if (t == -1)
        cout << "Error in Converting time";

    //cout<<"seconds since the Epoch: "<< (long) t;

    return (t);

}

int main(int argc, char *argv[]) {

    struct addrinfo hints, *remote_sock;
    int status, socketfd, httpport, pos;
    char *http_response, *hoststr, *pagestr, buffer[BUFSIZ + 1], port[] = "80";
    string str, str2, *ptr = &str2, host, httppage, url, *hostptr = &host, *pageptr = &httppage, last_mod, *last_modified = &last_mod, path, *pathptr = &path;
    time_t totalsecs;

    map<string, long long> url_date;
    map<string, bool> visited_info;

    if (argc < 1) {
        fprintf(stderr, "\nUsage: Crawler URLPrefix Filename\n");
        return 1;
    }

    //Parse URLprefix, extract hostname and http file info
    url = argv[1];
    url += argv[2];

    if (url.find("http") != string::npos) {
        parseurl(url, hostptr, pageptr, pathptr);
    }


    Q.push(url);
    Q.push(url);

    while (Q.empty() == 0) {


        url = Q.front();
        //cout<<url;

        /* if(url.find(path) == string::npos)      {
             url=Q.front();
             Q.pop();
             continue;
         }
         */


        if (visited_info[url] == 1 || (Q.size() > 2 && url.size() > 13)) {
            url = Q.front();
            Q.pop();
            continue;
        }


        //Set the hints file for action
        memset(&hints, 0, sizeof (hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;


        //build the details of remote socket automagically
        if ((status = getaddrinfo(hoststr, port, &hints, &remote_sock)) != 0) {
            fprintf(stderr, "error at getaddrinfo: %s\n", gai_strerror(status));
            continue;
            return 2;
        }

        //create socket, get the socket descriptor
        if ((socketfd = socket(remote_sock->ai_family, remote_sock->ai_socktype, remote_sock->ai_protocol)) == -1) {
            perror("\nClient: Error in creating a socket %d\n");
            return 3;
        }


        //connect to the socket
        if (status = connect(socketfd, remote_sock->ai_addr, remote_sock->ai_addrlen) == -1) {
            perror("\nClient: Error in Connecting %d\n");
            return 3;
        }

        if (Q.size() > 2 || url == "page1.html") {
            httppage = path;
            httppage += "/";
            httppage += url;
        }

        //cout << "URL: " << url << "\tHost: " << host << "\tPath: " << path << "\tPage: " << httppage << endl;

        hoststr = (char *) host.c_str();
        pagestr = (char *) httppage.c_str();

        //build the query
        http_response = build_http_query(hoststr, pagestr);
        //fprintf(stderr, "Query is:\n<Begin>\n%s<End>\n", http_response);

        //send query
        send_http_query(socketfd, http_response, status);

        //receive html page
        str = receive_html(socketfd, buffer, status, last_modified);

        if (str == "") {
            Q.pop();
            continue;
        }
        int i = 0;

        while (i != string::npos) {
            i = hrefs(str, i, ptr);

            if (i != string::npos)
                Q.push(str2);
        }

        totalsecs = time_seconds(last_mod);


        //url_date.insert(pair<string,long>(url, totalsecs));
        //visited_info.insert(pair<string,bool>(url, 1));
        url_date[url] = totalsecs;
        visited_info[url] = 1;

        //cout << str;
        // cout << "\nNumber of Links: " << Q.size() << endl << "Last Modified: " << last_mod << endl<<"Size of List: "<<url_date.size()<<endl;


        free(http_response);
        close(socketfd);
        freeaddrinfo(remote_sock);

        Q.pop();

    }

    map<string, long long>::iterator temp;
    map<string, long long>::iterator end = url_date.end();
    map<string, long long>::iterator start = url_date.begin();

    long int max;
    max = start->second;

    while (start != end) {

        //cout<<start->first<<"\t"<<start->second<<endl;

        //cout<<max<<endl;
        if ((start->second) > max) {
            max = (start->second);
            temp = start;
        }

        start++;
    }

    end = url_date.end();
    start = url_date.begin();

    //cout<<max;

    while (start != end) {
        if (start->second == max) {
            string result = start->first, tmpres = result;

            if (result.find("http", 0) != string::npos) {

            } else {
                result = path;
                result += "/";
                result += tmpres;
            }

            cout << "\n\nLast Modified URL " << result << endl;

        }
        start++;
    }


    return 0;
}
