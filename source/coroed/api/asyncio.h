#pragma once

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "task.h"

/**
 * Синтаксический сахар для использования в теле файбера.
 */
#define ASYNC_READ(fd, buf, count) async_read(__self, (fd), (buf), (count))
#define ASYNC_WRITE(fd, buf, count) async_write(__self, (fd), (buf), (count))

/**
 * Асинхронное чтение и запись.
 * Возвращает количество обработанных байтов или `-errno` в случае ошибки.
 */
ssize_t async_read(struct task* self, int fd, void* buf, size_t count);
ssize_t async_write(struct task* self, int fd, const void* buf, size_t count);