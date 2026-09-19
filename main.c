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

enum {
    read_buffer_size = 1024 * 1024
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
        *self->values = (struct values) {
            .minimum = value,
            .maximum = value
        };
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

struct optional_char {
    bool present;
    char value;
};

enum line_state {
    location_name,
    location_measurement
};

struct collector {
    int fd;
    struct node *data;
    size_t buffer_position;
    size_t buffer_length;
    char buffer[read_buffer_size];
};

struct optional_char collector_read_optional_char(struct collector *self) {
    if (self->buffer_position == self->buffer_length) {
        ssize_t result = read(self->fd, self->buffer, sizeof(self->buffer));
        if (result == 0) {
            return (struct optional_char) {
                .present = false
            };
        }
        if (result < 0) {
            perror("Failed to refill read buffer");
            exit(1);
        }
        self->buffer_position = 0;
        self->buffer_length = (size_t)result;
    }
    return (struct optional_char) {
        .present = true,
        .value = self->buffer[self->buffer_position++]
    };
}

char collector_read_char(struct collector *self) {
    struct optional_char result = collector_read_optional_char(self);
    if (!result.present) {
        fprintf(stderr, "Unexpected EOF");
        exit(1);
    }
    return result.value;
}

uint8_t read_digit(char c) {
    if (c < '0' || c > '9') {
        fprintf(stderr, "Not a digit: %d", c);
        exit(1);
    }
    return c - '0';
}

bool collector_process_line(struct collector *self) {
    struct optional_char first_char = collector_read_optional_char(self);
    if (!first_char.present) {
        return false;
    }
    char c = first_char.value;
    struct node *cursor = self->data;
    while (c != ';') {
        cursor = node_subnode(cursor, c);
        c = collector_read_char(self);
    }
    c = collector_read_char(self);
    int64_t value = 0;
    bool negative = c == '-';
    if (negative) {
        c = collector_read_char(self);
    }
    while (c != '.') {
        uint8_t digit = read_digit(c);
        value *= 10;
        value += digit;
        c= collector_read_char(self);
    }
    c = collector_read_char(self);
    uint8_t post_point_digit = read_digit(c);
    value *= 10;
    value += post_point_digit;
    if (negative) {
        value = -value;
    }
    struct optional_char line_end = collector_read_optional_char(self);
    if (line_end.present && line_end.value != '\n') {
        fprintf(stderr, "Expected newline or EOF: %d", line_end.value);
        exit(1);
    }
    node_update(cursor, value);
    return true;
}

int main(void) {
    int fd = open("measurements_1m.txt", O_RDONLY);
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
