#include <stdio.h>
#include <stdlib.h>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <arpa/inet.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>

#define ACTION_MAP_REMOTE (1<<0)
#define ACTION_MAP_LOCAL  (1<<1)
#define ACTION_SYNC_DOWN  (1<<2)
#define ACTION_SYNC_UP	  (1<<3)

int print_intro();
int sync_up(LIBSSH2_SFTP *sftp, FILE* mapinput, const char* remoteroot, unsigned long host, char* username);
int sync_down(FILE* mapinput, const char* localroot, unsigned long host, char* username);
int write_remote_map(LIBSSH2_SFTP *sftp, FILE* outputfile, const char* maproot);
int write_local_map(FILE* outputfile, const char* maproot);

int libssh2_userauth_publickey_auto(LIBSSH2_SESSION *session, const char *user, const char *passphrase);

int main(int argc , char *argv[]){
	unsigned long hostaddr = 0;
	int i, rc, sock;
	struct sockaddr_in sin;
	LIBSSH2_SESSION *session;
	LIBSSH2_SFTP *sftp_session;
	LIBSSH2_SFTP_HANDLE *sftp_handle;
		
	char username[255];
	username[0] = 0;
	char* path = "/";
	char* mapfile = "map";
    
	unsigned long action = 0;
	
	for(i = 1; i < argc; i++){
		if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0){
			if(i+1 < argc)
				hostaddr = inet_addr(argv[i+1]);
		}else if(strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--user") == 0){
			if(i+1 < argc)
				strcpy(username, argv[i+1]);
		}else if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--directory") == 0){
			if(i+1 < argc){
				path = argv[i+1];
				if(path[strlen(path) - 1] == '/')
					path[strlen(path) - 1] = 0;
			}
		}else if(strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mapfile") == 0){
			if(i+1 < argc)
				mapfile = argv[i+1];
		}else if(strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--action") == 0){
			if(i+1 < argc){
				if(strcmp(argv[i+1], "mapremote") == 0)
					action = ACTION_MAP_REMOTE;
				else if(strcmp(argv[i+1], "maplocal") == 0)
					action = ACTION_MAP_LOCAL;
				else if(strcmp(argv[i+1], "syncdown") == 0)
					action = ACTION_SYNC_DOWN;
				else if(strcmp(argv[i+1], "syncup") == 0)
					action = ACTION_SYNC_UP;
			}
		}
	}
	if(action == 0){
		fprintf(stderr, "No action specified.");
		print_intro();
		return 1;
	}
	
	//map local
	if(action & ACTION_MAP_LOCAL){
		FILE *f = fopen(mapfile, "w");
		if(f == NULL){
			fprintf(stderr, "Error opening file");
			return 1;
		}
		fprintf(f, "r:%s/\n", path);
		write_local_map(f, path);
		fclose(f);
		return 0;
	}
	
	if(hostaddr == 0){
		fprintf(stderr, "No host specified.");
		print_intro();
		return 1;
	}
	if(username[0] == 0){
		printf("Username: ");
		scanf("%255s", username);
	}
	
	if((rc = libssh2_init(0)) != 0){
		fprintf(stderr, "libssh2 init failed (%d)\n", rc);
		return 1;
	}
	sock = socket(AF_INET, SOCK_STREAM, 0);
	
	sin.sin_family = AF_INET;
	sin.sin_port = htons(22);
	sin.sin_addr.s_addr = hostaddr;
	if(connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0){
		fprintf(stderr, "failed to connect!\n");
		return 1;
	}
	
	if(!(session = libssh2_session_init())){
		fprintf(stderr, "Error initializing session.\n");
		return 1;
	}
	
	if((rc = libssh2_session_handshake(session, sock))){
		fprintf(stderr, "failed to establish SSH connection (%d)n", rc);
		return 1;
	}
	
	if(libssh2_userauth_publickey_auto(session, username,"")){
		fprintf(stderr, "Could not authenticate properly.\n");
		//TODO: proper deinitialization
		return 1;
	}
	
	if(!(sftp_session = libssh2_sftp_init(session))){
		fprintf(stderr, "Error initializing SFTP\n");
		//TODO: proper deinit
		return 1;
	}
    libssh2_session_set_blocking(session, 1);
	
	if(action & ACTION_MAP_REMOTE){
		FILE *f = fopen(mapfile, "w");
		if(f == NULL){
			fprintf(stderr, "Error opening file");
			return 1;
		}
		fprintf(f, "r:%s/\n", path);
		write_remote_map(sftp_session, f, path);
		fclose(f);
	}else if(action & ACTION_SYNC_DOWN){
		FILE *f = fopen(mapfile, "r");
		if(f == NULL){
			fprintf(stderr, "Error opening file");
			return 1;
		}
		sync_down(f, path, hostaddr, username);
		fclose(f);
			
	}else if(action & ACTION_SYNC_UP){
		FILE *f = fopen(mapfile, "r");
		if(f == NULL){
			fprintf(stderr, "Error opening file");
			return 1;
		}
		sync_up(sftp_session, f, path, hostaddr, username);
		fclose(f);
	}
	libssh2_sftp_shutdown(sftp_session);
 
   	libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
    libssh2_session_free(session);
	return 0;
}

int sync_up(LIBSSH2_SFTP *sftp, FILE* mapinput, const char* remoteroot, unsigned long host, char* username){
	int maprootlen;
	char line[1024];
	const char delim[2] = "/";
	while(fgets(line, sizeof(line), mapinput)){
		line[strlen(line)-1] = 0; //remove newline
		if(line[0] == '1'){ //should be synced?
			memmove(line, line+2, strlen(line)); // remove first two chars - "1:"
			
			char *tmp = strdup(line);
			memmove(tmp, tmp + maprootlen, strlen(tmp)); //remove root part of path
			
			char *remotepath = malloc(strlen(remoteroot) + strlen(tmp) + 2);
			strcpy(remotepath, remoteroot);
			if(remotepath[strlen(remotepath) -1] != delim[0])
				strcat(remotepath, delim);
			strcat(remotepath, tmp);
			free(tmp);
			//line = local path
			
			//create necessary directories remotely
			char* builder = malloc(strlen(remotepath));
			char* tmppath = strdup(remotepath);
			char* dir = strtok(tmppath, delim);
			
			strcpy(builder, (remotepath[0] == delim[0])?delim:"");
			
			char* dest;
			while(dir != NULL){
				dest = strstr(remotepath, dir);
				if(dest - remotepath + strlen(dir) != strlen(remotepath)){
					strcat(builder, dir);
					strcat(builder, delim);
					LIBSSH2_SFTP_ATTRIBUTES st;
					if (libssh2_sftp_stat(sftp, builder, &st) < 0) {
						libssh2_sftp_mkdir(sftp, builder, 0700);
					}
				}
				dir = strtok(NULL, delim);
			}
			free(tmppath);
			free(builder);
			
			//build ssh string
			struct in_addr hostaddr;
			hostaddr.s_addr = host;
			char *remotestring = malloc(strlen(username) + strlen(inet_ntoa(hostaddr)) + strlen(remotepath) + 3);
			strcpy(remotestring, username);
			strcat(remotestring, "@");
			strcat(remotestring, inet_ntoa(hostaddr));
			strcat(remotestring, ":");
			strcat(remotestring, remotepath);
		
			//build command string
			char* commandstring = malloc(strlen(remotestring) + strlen(line) + 22);//22 characters for command
			strcpy(commandstring, "rsync -vrtzD -e ssh ");
			strcat(commandstring, line);
			strcat(commandstring, " ");
			strcat(commandstring, remotestring);
			//printf("command:%s\n", commandstring);			
			
			//exec command with system call
			system(commandstring);						

			free(remotestring);
			free(remotepath);
			free(commandstring);
			
			
		}else if(line[0] == 'r'){
			memmove(line, line+2, strlen(line));
			if(line[strlen(line) - 1] != '/')
				strcat(line, "/");
			maprootlen = strlen(line);
		}
	}
}

int sync_down(FILE* mapinput, const char* localroot, unsigned long host, char* username){
	int maprootlen;
	char line[1024];
	const char delim[2] = "/";
    while (fgets(line, sizeof(line), mapinput)) {
		line[strlen(line)-1] = 0;
        if(line[0] == '1'){
			memmove(line, line+2, strlen(line));
			
			//build local path string
			char *tmp = strdup(line);
			memmove(tmp, tmp + maprootlen, strlen(tmp));
			
			char *localpath = malloc(strlen(localroot) + strlen(tmp) + 2);
			strcpy(localpath, localroot);
			if(localpath[strlen(localpath) - 1] != delim[0])
				strcat(localpath, delim);
			strcat(localpath, tmp);
			free(tmp);
			//line = remote path, localpath = local path, duh
			
			//create necessary directories
			char* builder = malloc(strlen(localpath));
			char* tmppath = strdup(localpath);
			char* dir = strtok(tmppath, delim);
			
			strcpy(builder, (localpath[0] == delim[0])?delim:"");
			
			char* dest;
			while(dir != NULL){
				dest = strstr(localpath, dir);
				if(dest - localpath + strlen(dir) != strlen(localpath)){
					strcat(builder, dir);
					strcat(builder, delim);
					struct stat st = {0};
					if (stat(builder, &st) == -1) {
						mkdir(builder, 0700);
					}
				}
				dir = strtok(NULL, delim);
			}
			free(tmppath);
			free(builder);
			
			//build ssh string
			struct in_addr hostaddr;
			hostaddr.s_addr = host;
			char *remotestring = malloc(strlen(username) + strlen(inet_ntoa(hostaddr)) + strlen(line) + 3);
			strcpy(remotestring, username);
			strcat(remotestring, "@");
			strcat(remotestring, inet_ntoa(hostaddr));
			strcat(remotestring, ":");
			strcat(remotestring, line);
		
			//build command string
			char* commandstring = malloc(strlen(remotestring) + strlen(localpath) + 22);//22 characters for command
			strcpy(commandstring, "rsync -vrtzD -e ssh ");
			strcat(commandstring, remotestring);
			strcat(commandstring, " ");
			strcat(commandstring, localpath);
			//printf("command:%s\n", commandstring);			
			
			//exec command with system call
			system(commandstring);						

			free(remotestring);
			free(localpath);
			free(commandstring);
		}else if(line[0] == 'r'){
			memmove(line, line+2, strlen(line));
			if(line[strlen(line) - 1] != '/')
				strcat(line, "/");
			maprootlen = strlen(line);
		}
    }
}

int write_remote_map(LIBSSH2_SFTP *sftp, FILE* outputfile, const char* maproot){
	int rc;
    LIBSSH2_SFTP_HANDLE *sftp_handle;
    
	if(!(sftp_handle = libssh2_sftp_opendir(sftp, maproot))){
		fprintf(stderr, "Unable to open dir with SFTP\n");
		return 1;
	}
	
	fprintf(outputfile, "0:%s/\n", maproot);
				
	do{
		char mem[1024];
        LIBSSH2_SFTP_ATTRIBUTES attrs;
        
        if((rc = libssh2_sftp_readdir(sftp_handle, mem, sizeof(mem), &attrs) > 0)){
			if(strcmp(mem, "..") != 0 && strcmp(mem, ".") != 0){
				
				char *tmp = strdup(mem);
				strcpy(mem, maproot);
				if(mem[strlen(mem) - 1] != '/')
					strcat(mem, "/");
				strcat(mem, tmp);
				free(tmp);
				
					
				if(LIBSSH2_SFTP_S_ISDIR(attrs.permissions)){
					write_remote_map(sftp, outputfile, mem);
				} else{
					fprintf(outputfile, "0:%s\n", mem);
				}
				printf(".");
			}
		}
	}while(rc > 0);
	libssh2_sftp_closedir(sftp_handle);
	return 0;
}

int write_local_map(FILE* outputfile, const char* maproot){
	DIR *d;
	struct dirent *dir;
	char mem[1024];
	struct stat statbuf;
	d = opendir(maproot);
	
	
	if(d){
		fprintf(outputfile, "0:%s/\n", maproot);
		while((dir = readdir(d)) != NULL){
			if(strcmp(dir->d_name, "..") != 0 && strcmp(dir->d_name, ".") != 0){
				
				strcpy(mem, maproot);
				if(mem[strlen(mem) - 1] != '/')
					strcat(mem, "/");
				strcat(mem, dir->d_name);
				
				if(stat(mem, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)){
					write_local_map(outputfile, mem);
				}else{
					fprintf(outputfile, "0:%s\n", mem);
				}
			}
		}
		closedir(d);
	}
	return 0;
}

int libssh2_userauth_publickey_auto(LIBSSH2_SESSION *session, const char *username, const char *passphrase){
	
	DIR *d;
	struct dirent *dir;
	
	struct passwd *pw = getpwuid(getuid());
	//tilde doesn't work for some reason...
	char *sshdir = pw->pw_dir;
	strcat(sshdir, "/.ssh/");
	d = opendir(sshdir);
	if(d){
		while((dir = readdir(d)) != NULL){
			if(strcmp(dir->d_name, "known_hosts") != 0 && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0){
				const char *extension = &(dir->d_name[strlen(dir->d_name)-4]);
				if(strcmp(extension, ".pub") != 0){
					
					char *priv = malloc(strlen(sshdir) + strlen(dir->d_name) + 1);
					strcpy(priv, sshdir);
					strcat(priv, dir->d_name);
					
					char *pub = malloc(strlen(sshdir) + strlen(dir->d_name) + 5);
					strcpy(pub, sshdir);
					strcat(pub, dir->d_name);
					strcat(pub, ".pub");
					
					if(access( pub, F_OK ) != -1 ) {
						if(libssh2_userauth_publickey_fromfile(session, username, pub, priv,"") == 0){
							free(priv);
							free(pub);
							closedir(d);
							return 0; //success
						}
					}
					free(priv);
					free(pub);
				}
			}
			
			
		}
		closedir(d);
	}
	return -1;
}

int print_intro(){
	printf("\n***********************************************************************************\n");
	printf("freshsync v0.6 by frequem\n");
	printf("Parameters:\n");
	printf("-h, --host: remote host IP e.g. 192.168.0.3\n");
	printf("-u, --user: remote ssh username e.g. root\n");
	printf("-d, --directory: remote or local directory path e.g. /home/user/files (default: /)\n");
	printf("-m, --mapfile: output/input file for local or remote map e.g. remote.map (default: map)\n");
	printf("-a, --action: action you want to perform (mapremote|maplocal|syncdown|syncup)\n");
	printf("Examples:\n");
	printf("freshsync -h 192.168.0.3 -u user -d /media/hdd/sync -m localmap.txt -a syncup\n");
	printf("freshsync -h 192.168.0.3 -u user -d /home/user/backup -m remotemap.txt -a syncdown\n");
	printf("freshsync -h 192.168.0.3 -u user -d /media/hdd/sync -m remotemap.txt -a mapremote\n");
	printf("freshsync -d /home/user/backup -m localmap.txt -a maplocal\n");
	printf("***********************************************************************************\n");
	printf("Your server should be set up to not require a password from the machine that is running this application.\n");
	printf("There has to be private + public(.pub) key present in ~/.ssh/.\n");
	printf("***********************************************************************************\n");
	return 0;
}
