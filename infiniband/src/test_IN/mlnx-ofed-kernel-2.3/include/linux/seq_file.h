#ifndef _COMPAT_SEQ_FILE_
#define _COMPAT_SEQ_FILE_

#include_next <linux/seq_file.h>

#if defined(COMPAT_VMWARE)
/**
 * seq_get_buf - get buffer to write arbitrary data to
 * @m: the seq_file handle
 * @bufp: the beginning of the buffer is stored here
 *
 * Return the number of bytes available in the buffer, or zero if
 * there's no space.
 */
static inline size_t seq_get_buf(struct seq_file *m, char **bufp)
{
        BUG_ON(m->count > m->size);
        if (m->count < m->size)
                *bufp = m->buf + m->count;
        else
                *bufp = NULL;

        return m->size - m->count;
}

/**
 * seq_commit - commit data to the buffer
 * @m: the seq_file handle
 * @num: the number of bytes to commit
 *
 * Commit @num bytes of data written to a buffer previously acquired
 * by seq_buf_get.  To signal an error condition, or that the data
 * didn't fit in the available space, pass a negative @num value.
 */
static inline void seq_commit(struct seq_file *m, int num)
{
        if (num < 0) {
                m->count = m->size;
        } else {
                BUG_ON(m->count + num > m->size);
                m->count += num;
        }
}
#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_SEQ_FILE_ */
