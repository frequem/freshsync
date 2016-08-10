# FreshSync
A miniature RSync wrapper written in C

Your server should be set up to not require a password from the machine that is running this application.

There has to be private + public(.pub) key present in ~/.ssh/

#Building

To build, run:

    make
    
You need libssh2 installed on your machine!

#Parameters

-h, --host: remote host IP e.g. 192.168.0.3

-u, --user: remote ssh username e.g. root

-d, --directory: remote or local directory path e.g. /home/user/files (default: /)

-m, --mapfile: output/input file for local or remote map e.g. remote.map (default: map)

-a, --action: action you want to perform (mapremote|maplocal|syncdown|syncup)

#Mapfiles

Both map files (local and remote) follow this pattern:
	
	r:/media/hdd/sync/
	0:/media/hdd/sync/
	0:/media/hdd/sync/test/
	1:/media/hdd/sync/test/abc/
	0:/media/hdd/sync/test/abc/file1.txt
	0:/media/hdd/sync/test/abc/file2.txt

Directories always end with a /.

r indicates the root directory.

1 indicates that a directory or file will be synced, only the top directory must be marked with a 1.

0 indicated that a directory or file will not be synced, or that a higher directory is already marked with a 1.

#Examples

	freshsync -h 192.168.0.3 -u user -d /media/hdd/sync -m localmap.txt -a syncup
	freshsync -h 192.168.0.3 -u user -d /home/user/backup -m remotemap.txt -a syncdown
	freshsync -h 192.168.0.3 -u user -d /media/hdd/sync -m remotemap.txt -a mapremote
	freshsync -d /home/user/backup -m localmap.txt -a maplocal
    
#License

This is under the GPLv3 license.
