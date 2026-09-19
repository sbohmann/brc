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

void node_update(struct node *self, int64_t value) {
    struct values *values = self-> values;
    if (values == nullptr) {
        values = malloc(sizeof(struct values));
        if (values == nullptr) {
            perror("Failed to allocate values for a node");
            exit(1);
        }
        self->values = values;
        *self->values = (struct values) {};
    }
    if (value < values->minimum) {
        values->minimum = value;
    }
    if (value > values->maximum) {
        values->maximum = value;
    }
    values->sum += value;
    ++values->number;
}

char read_char(int fd) {
    char c;
    ssize_t result = read(fd, &c, 1);
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

uint8_t read_digit(char c) {
    if (c < '0' || c > '9') {
        fprintf(stderr, "Not a digit: %d", c);
        exit(1);
    }
    return c - '0';
}

bool collector_process_line(struct collector *self) {
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
    c = read_char(self->fd);
    int64_t value = 0;
    while (c != '.') {
        uint8_t digit = read_digit(c);
        value *= 10;
        value += digit;
        c= read_char(self->fd);
    }
    c = read_char(self->fd);
    uint8_t post_point_digit = read_digit(c);
    value *= 10;
    value += post_point_digit;
    node_update(cursor, value);
    return true;
}

int main(void) {
    int fd = open("measurements.txt", O_RDONLY);
    if (fd == -1) {
        perror("Error opening measurements.txt");
        return 1;
    }

    struct node root = {};

    struct collector collector = {
        .fd = fd,
        .data = &root
    };

    while (collector_process_line(&collector));

    close(fd);
    return 0;
}
