#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

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

        if (stat(path, &fileStat) < 0) {
            perror("Error getting file stat");
            closedir(dir);
            return;
        }

        if (S_ISDIR(fileStat.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            traverseDirectory(path, outputFile);
        }

        char buffer[4096]; // Increased buffer size to accommodate all fields

        snprintf(buffer, sizeof(buffer),
                 "File: %s\n"
                 "File type: %d\n"
                 "Device ID: %ld\n"
                 "Inode number: %ld\n"
                 "Number of hard links: %ld\n"
                 "User ID of owner: %ld\n"
                 "Group ID of owner: %ld\n"
                 "Device ID (if special file): %ld\n"
                 "Total size, in bytes: %ld\n"
                 "Last status change: %s"
                 "Last file access: %s"
                 "Last file modification: %s"
                 "\n",
                 path, fileStat.st_mode, (long)fileStat.st_dev, (long)fileStat.st_ino, (long)fileStat.st_nlink,
                 (long)fileStat.st_uid, (long)fileStat.st_gid, (long)fileStat.st_rdev, (long)fileStat.st_size,
                 ctime(&fileStat.st_ctime), ctime(&fileStat.st_atime), ctime(&fileStat.st_mtime));

        write(outputFile, buffer, strlen(buffer));
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s Directory_Path\n", argv[0]);
        return 1;
    }

    char snapshotPath[1024];
    snprintf(snapshotPath, sizeof(snapshotPath), "%s/snapshot.txt", argv[1]);

    int outputFile = open(snapshotPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (outputFile < 0) {
        perror("Error opening output file");
        return 1;
    }

    traverseDirectory(argv[1], outputFile);

    close(outputFile);

    return 0;
}
