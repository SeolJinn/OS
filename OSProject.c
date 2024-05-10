#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <ctype.h>
#include <libgen.h>

#define MAX_FILES 1024 
#define BUFFER_SIZE 4096 

struct FileStat {
    char *path;
    int fileType;
    long deviceID;
    long inodeNumber;
    long numHardLinks;
    long ownerUserID;
    long ownerGroupID;
    long deviceIDSpecialFile;
    long totalSize;
    char *lastStatusChange;
    char *lastFileAccess;
    char *lastFileModification;
};

enum DifferenceType {
    NO_DIFFERENCE,
    FILE_ADDED,
    FILE_DELETED,
    FILE_RENAMED,
    FILE_EDITED,
    FILE_MOVED
};

struct Difference {
    enum DifferenceType type;
    char *path;
};

void readSnapshotFile(FILE *snapshotFile, struct FileStat *fileStats, int *fileCount) {
    char line[BUFFER_SIZE];

    while (fgets(line, BUFFER_SIZE, snapshotFile) != NULL) {
        fileStats[*fileCount].path = strdup(line); 
        fileStats[*fileCount].fileType = atoi(fgets(line, BUFFER_SIZE, snapshotFile));
        fileStats[*fileCount].deviceID = atol(fgets(line, BUFFER_SIZE, snapshotFile));
        fileStats[*fileCount].inodeNumber = atol(fgets(line, BUFFER_SIZE, snapshotFile));
        fileStats[*fileCount].numHardLinks = atol(fgets(line, BUFFER_SIZE, snapshotFile));
        fileStats[*fileCount].ownerUserID = atol(fgets(line, BUFFER_SIZE, snapshotFile));
        fileStats[*fileCount].ownerGroupID = atol(fgets(line, BUFFER_SIZE, snapshotFile));
        fileStats[*fileCount].deviceIDSpecialFile = atol(fgets(line, BUFFER_SIZE, snapshotFile));
        fileStats[*fileCount].totalSize = atol(fgets(line, BUFFER_SIZE, snapshotFile));
        fileStats[*fileCount].lastStatusChange = strdup(fgets(line, BUFFER_SIZE, snapshotFile));
        fileStats[*fileCount].lastFileAccess = strdup(fgets(line, BUFFER_SIZE, snapshotFile));
        fileStats[*fileCount].lastFileModification = strdup(fgets(line, BUFFER_SIZE, snapshotFile));

        (*fileCount)++; 
    }
}


bool isSameFile(struct FileStat *file1, struct FileStat *file2) {
    // Compare all attributes except the path
    return (file1->fileType == file2->fileType &&
            file1->deviceID == file2->deviceID &&
            file1->inodeNumber == file2->inodeNumber &&
            file1->numHardLinks == file2->numHardLinks &&
            file1->ownerUserID == file2->ownerUserID &&
            file1->ownerGroupID == file2->ownerGroupID &&
            file1->deviceIDSpecialFile == file2->deviceIDSpecialFile &&
            file1->totalSize == file2->totalSize &&
            strcmp(file1->lastStatusChange, file2->lastStatusChange) == 0 &&
            strcmp(file1->lastFileAccess, file2->lastFileAccess) == 0 &&
            strcmp(file1->lastFileModification, file2->lastFileModification) == 0);
}

void findDifferences(struct FileStat *newFileStat, int newFileCount, struct FileStat *existingFileStat, int existingFileCount) {
    struct Difference differences[MAX_FILES];
    int differenceCount = 0;

    //Flag to mark existing files as matched. If not matched, it is deleted
    bool existingMatched[existingFileCount];
    memset(existingMatched, false, sizeof(existingMatched));

    //Compare each file in the new snapshot with the files in the existing snapshot
    for (int i = 0; i < newFileCount; i++) {
        int found = 0;
        for (int j = 0; j < existingFileCount; j++) {
            if (strcmp(newFileStat[i].path, existingFileStat[j].path) == 0) {
                found = 1;

                // Check if file attributes other than path have changed
                if (!isSameFile(&newFileStat[i], &existingFileStat[j])) {
                    differences[differenceCount].type = FILE_EDITED;
                    differences[differenceCount].path = newFileStat[i].path;
                    differenceCount++;
                }

                // Mark the existing file as matched
                existingMatched[j] = true;
                break;
            }
        }

        // If the file is not found in the existing snapshot, it's either added, moved, or renamed
        if (!found) {
            int MoveOrRename = 0;
            
            for(int j = 0; j < existingFileCount; j++) 
            {
                //Check if inode number exists
                if(newFileStat[i].inodeNumber == existingFileStat[j].inodeNumber) 
                {
                    //Extract filename after last '/'
                    char *newFileName = strrchr(newFileStat[i].path, '/');
                    char *existingFileName = strrchr(existingFileStat[j].path, '/');
                    if(strcmp(newFileName, existingFileName) == 0) 
                    {
                        differences[differenceCount].type = FILE_MOVED;
                        differences[differenceCount].path = newFileStat[i].path;
                        existingMatched[j] = true;
                    }
                    else 
                    {
                        differences[differenceCount].type = FILE_RENAMED;
                        differences[differenceCount].path = newFileStat[i].path;
                        existingMatched[j] = true;
                    }
                    MoveOrRename = 1;
                    break;
                }
            }

            if(MoveOrRename == 0) 
            {
                differences[differenceCount].type = FILE_ADDED;
                differences[differenceCount].path = newFileStat[i].path;
            }

            differenceCount++;
        }
    }

    // Any files remaining in the existing snapshot list are deleted
    for (int i = 0; i < existingFileCount; i++) {
        if (!existingMatched[i]) {
            differences[differenceCount].type = FILE_DELETED;
            differences[differenceCount].path = existingFileStat[i].path;
            differenceCount++;
        }
    }

    // Print the differences
    for (int i = 0; i < differenceCount; i++) {
        switch (differences[i].type) {
            case FILE_ADDED:
                printf("File added: %s", differences[i].path);
                break;
            case FILE_DELETED:
                printf("File deleted: %s", differences[i].path);
                break;
            case FILE_EDITED:
                printf("File edited: %s", differences[i].path);
                break;
            case FILE_MOVED:
                printf("File moved: %s", differences[i].path);
                break;
            case FILE_RENAMED:
                printf("File renamed: %s", differences[i].path);
                break;
            default:
                break;
        }
    }

    if (differenceCount == 0) 
        printf("No differences found.\n");
}


void compareSnapshots(const char *basePath, const char *newSnapshotPath, const char *existingSnapshotPath) {
    FILE *newSnapshotFile = fopen(newSnapshotPath, "r");
    if (newSnapshotFile == NULL) {
        perror("Error opening new snapshot file");
        return;
    }

    FILE *existingSnapshotFile = fopen(existingSnapshotPath, "r");
    if (existingSnapshotFile == NULL) {
        perror("Error opening existing snapshot file");
        fclose(newSnapshotFile);
        return;
    }

    struct FileStat newFileStat[MAX_FILES];
    struct FileStat existingFileStat[MAX_FILES];

    int newFileCount = 0;
    int existingFileCount = 0;

    readSnapshotFile(newSnapshotFile, newFileStat, &newFileCount);
    readSnapshotFile(existingSnapshotFile, existingFileStat, &existingFileCount);

    fclose(newSnapshotFile);
    fclose(existingSnapshotFile);

    findDifferences(newFileStat, newFileCount, existingFileStat, existingFileCount);
}

void traverseDirectory(const char *basePath, int outputFile, int pipeWrite) {
    DIR *dir = opendir(basePath);
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    struct dirent *entry;
    struct stat fileStat;

    while ((entry = readdir(dir)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name);

        // To skip . .. as well as the snapshot files
        char basePathCopy[1024];
        strcpy(basePathCopy, basePath);
        char SnapshotName[1024];
        snprintf(SnapshotName, sizeof(SnapshotName), "SNAPSHOT_%s.txt", strtok(basePathCopy, "/"));
        char tempSnapshotName[1024];
        snprintf(tempSnapshotName, sizeof(tempSnapshotName), "SNAPSHOT_TEMP_%s.txt", strtok(basePathCopy, "/"));
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, SnapshotName) == 0 || strcmp(entry->d_name, tempSnapshotName) == 0) {
            continue;
        }

        if (stat(path, &fileStat) < 0) {
            perror("Error getting file stat");
            closedir(dir);
            return;
        }

        if (S_ISDIR(fileStat.st_mode)) {
            traverseDirectory(path, outputFile, pipeWrite);
        } else {
            // Write file information to output file
            char buffer[4096];
            snprintf(buffer, sizeof(buffer),
                     "%s\n%d\n%ld\n%ld\n%ld\n%ld\n%ld\n%ld\n%ld\n%s%s%s",
                     path, fileStat.st_mode, (long)fileStat.st_dev, (long)fileStat.st_ino, (long)fileStat.st_nlink,
                     (long)fileStat.st_uid, (long)fileStat.st_gid, (long)fileStat.st_rdev, (long)fileStat.st_size,
                     ctime(&fileStat.st_ctime), ctime(&fileStat.st_atime), ctime(&fileStat.st_mtime));

            write(outputFile, buffer, strlen(buffer));
            
            // Fork a child process to check each file
            pid_t pid = fork();
            if (pid == -1) {
                perror("Error forking child process");
                return;
            } else if (pid == 0) { // Child process
                close(outputFile);

                // Check for missing permissions
                if ((fileStat.st_mode & S_IRWXU) == 0 && (fileStat.st_mode & S_IRWXG) == 0 && (fileStat.st_mode & S_IRWXO) == 0) {
                    write(pipeWrite, path, strlen(path) + 1);
                    close(pipeWrite);
                    exit(0);
                }

                // Perform file syntactic analysis
                FILE *file = fopen(path, "r");
                if (file == NULL) {
                    perror("Error opening file");
                    exit(1);
                }

                char line[BUFFER_SIZE];
                int lineCount = 0;
                int wordCount = 0;
                int charCount = 0;
                bool containsKeyword = false;
                bool containsNonASCII = false;

                while (fgets(line, BUFFER_SIZE, file) != NULL) {
                    lineCount++;
                    charCount += strlen(line);
                    char *token = strtok(line, " \t\n");
                    while (token != NULL) {
                        wordCount++;
                        token = strtok(NULL, " \t\n");
                    }

                    // Check for dangerous keywords
                    if (strstr(line, "corrupted") != NULL || strstr(line, "dangerous") != NULL ||
                        strstr(line, "risk") != NULL || strstr(line, "attack") != NULL ||
                        strstr(line, "malware") != NULL || strstr(line, "malicious") != NULL) {
                        containsKeyword = true;
                    }

                    // Check for non-ASCII characters
                    for (int i = 0; i < strlen(line); i++) {
                        if (!isascii(line[i])) {
                            containsNonASCII = true;
                            break;
                        }
                    }
                }

                fclose(file);

                if (containsKeyword || containsNonASCII) {
                    write(pipeWrite, path, strlen(path) + 1);
                } else {
                    write(pipeWrite, "SAFE", strlen("SAFE") + 1);
                }

                close(pipeWrite);
                exit(0);
            }
        }
    }

    closedir(dir);
}

int fileExists(const char *filePath) {
    return access(filePath, F_OK) != -1;
}

int deleteFile(const char *filePath) {
    return remove(filePath);
}

int main(int argc, char *argv[]) {
    if (argc < 6 || strcmp(argv[1], "-o") != 0 || strcmp(argv[3], "-s") != 0) {
        printf("Usage: %s -o Output_Directory -s Isolated_Space Directory_Path1 Directory_Path2 ... Directory_PathN\n", argv[0]);
        return 1;
    }

    // Get the output directory and isolated space directory
    char outputDirectory[256];
    char isolatedSpace[256];
    strcpy(outputDirectory, argv[2]);
    strcat(outputDirectory, "/");
    strcpy(isolatedSpace, argv[4]);
    strcat(isolatedSpace, "/");

    for(int i = 5; i < argc; i++)
    {
        pid_t pid = fork(); //Fork a child process
        if(pid == -1)
        {
            perror("Error forking child process");
            return 1;
        }
        else if(pid == 0)
        {
            char SnapshotFileName[256];
            char SnapshotPath[2048];
            snprintf(SnapshotFileName, sizeof(SnapshotFileName), "SNAPSHOT_%s.txt", argv[i]);
            snprintf(SnapshotPath, sizeof(SnapshotPath), "%s%s", outputDirectory, SnapshotFileName);

            char tempSnapshotFileName[256];
            char tempSnapshotPath[2048];
            snprintf(tempSnapshotFileName, sizeof(tempSnapshotFileName), "SNAPSHOT_TEMP_%s.txt", argv[i]);
            snprintf(tempSnapshotPath, sizeof(tempSnapshotPath), "%s%s", outputDirectory, tempSnapshotFileName);

            if(fileExists(SnapshotPath))
            {
                printf("Existing snapshot found for directory %s. Comparing with the new snapshot...\n", argv[i]);

                int tempOutputFile = open(tempSnapshotPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if(tempOutputFile < 0)
                {
                    perror("Error creating temporary output file");
                    return 1;
                }

                traverseDirectory(argv[i], tempOutputFile, -1);
                close(tempOutputFile);

                compareSnapshots(argv[i], tempSnapshotPath, SnapshotPath);

                if(deleteFile(SnapshotPath) != 0)
                {
                    perror("Error deleting existing snapshot file");
                    return 1;
                }

                if(rename(tempSnapshotPath, SnapshotPath) != 0)
                {
                    perror("Error renaming temporary snapshot file");
                    return 1;
                }

                printf("Snapshot for directory %s updated successfully.\n", argv[i]);
            }
            else
            {
                printf("No existing snapshot found for directory %s. Creating a new snapshot...\n", argv[i]);

                int outputFile = open(SnapshotPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if(outputFile < 0)
                {
                    perror("Error creating output file");
                    return 1;
                }

                traverseDirectory(argv[i], outputFile, -1);
                close(outputFile);

                printf("Snapshot for directory %s created successfully.\n", argv[i]);   
            }

            // Check files for malicious content
            int pipeFD[2];
            if (pipe(pipeFD) == -1) {
                perror("Pipe creation failed");
                return 1;
            }

            pid_t checkPid = fork();
            if (checkPid == -1) {
                perror("Error forking child process");
                return 1;
            } else if (checkPid == 0) { // Child process 
                close(pipeFD[0]); // Close the read end of the pipe
                traverseDirectory(argv[i], -1, pipeFD[1]); // Do not write to any output file
                
                close(pipeFD[1]); // Close the write end of the pipe
                exit(0);
            } else { // Parent process
                close(pipeFD[1]); // Close the write end of the pipe
                char result[256];
                    ssize_t bytesRead;

                int maliciousFileCount = 0;
                while ((bytesRead = read(pipeFD[0], result, sizeof(result))) > 0) {
                    if(strcmp(result, "SAFE") != 0)
                    {
                        maliciousFileCount++;
                        
                        //Move to isolated space
                        char newFilePath[256];
                        snprintf(newFilePath, sizeof(newFilePath), "%s%s", isolatedSpace, basename(result));
                        if(rename(result, newFilePath) != 0)
                        {
                            perror("Error moving file to isolated space");
                            return 1;
                        }
                    }
                }
                close(pipeFD[0]); // Close the read end of the pipe

                if (maliciousFileCount > 0) {
                    printf("Child process with PID %d found %d file(s) with malicious content\n", getpid(), maliciousFileCount);
                } else {
                    printf("Child process with PID %d found no files with malicious content\n", getpid());
                }
            }

            exit(0);
        }
    }

    // Parent process waits for all child processes to finish
    int status;
    pid_t wpid;
    while ((wpid = waitpid(-1, &status, 0)) > 0) {
        if (WIFEXITED(status)) {
            printf("Child process with PID %d exited with status %d\n", wpid, WEXITSTATUS(status));
        } else {
            printf("Child process with PID %d terminated abnormally\n", wpid);
        }
    }

    return 0;
}
