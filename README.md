# FAT32

This application reads and performs commands on the contents of a FAT32 disk image. The goal of this project is to get a full picture of how files are stored and accessed on a disk drive. This application does not support FAT32 long named files, short versions of the long named files are used instead.

To build the application run `make` in the command terminal
the executable file produced is a file named `fat32`

This application is able to do three commands:
	`info` -  Prints info about the fat32 image
	`list` -  Lists all the files and directories on the disk image
	`get`  -  requires filepath to a file. The found file is stored onto the local computer
	
# usage
`prompt> ./fat32 [fat32 image] [command] [filepath]`

*`fat32image` is the name of the disk drive being parsed. `command` is one of the three commands that can be executed(info, list, get). `filepath` is only required for the `get` command.*

### info command
`./fat32 [fat32 image] [info]`

Prints out information about the file system. Information includes the drive name, amount of free space (including reserved section), usabale free space, and cluster size.

**Example** 
`./fat32 fat32disk info`

**output** 

Drive name:    ImADiskDrive

Free Space:    27886 kB

Usable Space:  34264 kB

Cluster Size:  1 Sector(s) 512 Bytes

## list command
`./fat32 [fat32 image] [list]`

The output returned will be a list of files/directories stored in the disk image. A directories contents will be displayed directly below it. Output is indented to display the hiearchy of contents in the file system. Reminder: Long names are abbreviated to its shorter form. 

**Example** 
`./fat32 fat32disk list`

**output** 

DIRECTORY: FOLDER1

&nbsp;&nbsp; FILE:  file1.TXT

&nbsp;&nbsp; FILE:  _filee~1.TXT

&nbsp;&nbsp; FILE:  file2.TXT

&nbsp;&nbsp; FILE:  file3.TXT

&nbsp;&nbsp; FILE:  thatexists.txt

DIRECTORY: FOLDER2

&nbsp;&nbsp; FILE:  289.TXT

&nbsp;&nbsp; FILE:  504.TXT

## get command

`./fat32 [fat32 image] [get] [filename]`

This command will locate `filename` in the disk drive and place the file onto the local directory.

**Example** 
`./fat32 fat32disk get /FOLDER1/thatexists.txt`
**output** 
File 'thatexists.txt' was succesful

**Example** 
`./fat32 fat32disk get /FOLDER1/doesnotexist.txt`
**output** 
No file/directory named 'doesnotexist.txt'

This application was build on Ubuntu 20.04.2 LTS
