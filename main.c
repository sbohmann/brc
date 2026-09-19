#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

struct values {
    int64_t minimum;
    int64_t maximum;
    int64_t sum;
    size_t number;
};

struct node {
    struct node *subnodes[256];
    struct values *values;
};

struct node * allocate_node(void) {
    struct node *result = malloc(sizeof(struct node));
    if (result == nullptr) {
        perror("Failed to allocate node");
        exit(1);
    }
    *result = (struct node) {};
    return result;
}

struct node * node_subnode(struct node *self, char c) {
    unsigned char index = (unsigned char)c;
    struct node *subnode = self->subnodes[index];
    if (subnode == nullptr) {
        subnode = allocate_node();
        self->subnodes[index] = subnode;
    }
    return subnode;
}

char read_char(int fd) {
    char c;
    int result = read(fd, &c, 1);
    if (result != 1) {
        perror("Failed to read next character");
        exit(1);
    }
    return c;
}

struct optional_char {
    bool present;
    char value;
};

struct optional_char read_optional_char(int fd) {
    char c;
    ssize_t result = read(fd, &c, 1);
    if (result == 0) {
        return (struct optional_char) {
            .present = false
        };
    }
    if (result < 0) {
        perror("Unexpected result");
        exit(1);
    }
    return (struct optional_char) {
        .present = true,
        .value = c
    };
}

enum line_state {
    location_name,
    location_measurement
};

struct collector {
    int fd;
    struct node *data;
};

bool process_line(struct collector *self) {
    struct optional_char first_char = read_optional_char(self->fd);
    if (!first_char.present) {
        return false;
    }
    char c = first_char.value;
    struct node *cursor = self->data;
    while (c != ';') {
        cursor = node_subnode(cursor, c);
        c = read_char(self->fd);
    }
}

int main(void) {
    int fd = open("measurements.txt", O_RDONLY);
    if (fd == -1) {
        perror("Error opening measurements.txt");
        return 1;
    }

    while (process_line(fd));

    close(fd);
    return 0;
}
