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

void traverseDirectory(const char *basePath, int outputFile) {
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

        // To skip . .. snapshot.txt and snapshot_temp.txt
        //TODO MODIFIY snapshot names to skip
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || 
            strcmp(entry->d_name, "snapshot.txt") == 0 || strcmp(entry->d_name, "snapshot_temp.txt") == 0) {
            continue;
        }

        if (stat(path, &fileStat) < 0) {
            perror("Error getting file stat");
            closedir(dir);
            return;
        }

        if (S_ISDIR(fileStat.st_mode)) {
            traverseDirectory(path, outputFile);
        }

        char buffer[4096];

        snprintf(buffer, sizeof(buffer),
                 "%s\n%d\n%ld\n%ld\n%ld\n%ld\n%ld\n%ld\n%ld\n%s%s%s",
                 path, fileStat.st_mode, (long)fileStat.st_dev, (long)fileStat.st_ino, (long)fileStat.st_nlink,
                 (long)fileStat.st_uid, (long)fileStat.st_gid, (long)fileStat.st_rdev, (long)fileStat.st_size,
                 ctime(&fileStat.st_ctime), ctime(&fileStat.st_atime), ctime(&fileStat.st_mtime));

        write(outputFile, buffer, strlen(buffer));
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
    if (argc < 4 || strcmp(argv[1], "-o") != 0) {
        printf("Usage: %s -o Output_Directory Directory_Path1 Directory_Path2 ... Directory_PathN\n", argv[0]);
        return 1;
    }

    // Get the output directory
    char outputDirectory[256];
    strcpy(outputDirectory, argv[2]);
    strcat(outputDirectory, "/");

    for(int i = 3; i < argc; i++)
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

                traverseDirectory(argv[i], tempOutputFile);
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

                traverseDirectory(argv[i], outputFile);
                close(outputFile);

                printf("Snapshot for directory %s created successfully.\n", argv[i]);   
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
