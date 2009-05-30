static inline unsigned int rgb_to_pixel8_dup(unsigned int r, unsigned int g,
                                             unsigned int b)
{
    unsigned int col;
    col = rgb_to_pixel8(r, g, b);
    col |= col << 8;
    col |= col << 16;
    return col;
}

static inline unsigned int rgb_to_pixel15_dup(unsigned int r, unsigned int g,
                                              unsigned int b)
{
    unsigned int col;
    col = rgb_to_pixel15(r, g, b);
    col |= col << 16;
    return col;
}

static inline unsigned int rgb_to_pixel15bgr_dup(unsigned int r, unsigned int g,
                                                 unsigned int b)
{
    unsigned int col;
    col = rgb_to_pixel15bgr(r, g, b);
    col |= col << 16;
    return col;
}

static inline unsigned int rgb_to_pixel16_dup(unsigned int r, unsigned int g,
                                              unsigned int b)
{
    unsigned int col;
    col = rgb_to_pixel16(r, g, b);
    col |= col << 16;
    return col;
}

static inline unsigned int rgb_to_pixel16bgr_dup(unsigned int r, unsigned int g,
                                                 unsigned int b)
{
    unsigned int col;
    col = rgb_to_pixel16bgr(r, g, b);
    col |= col << 16;
    return col;
}

static inline unsigned int rgb_to_pixel32_dup(unsigned int r, unsigned int g,
                                              unsigned int b)
{
    unsigned int col;
    col = rgb_to_pixel32(r, g, b);
    return col;
}

static inline unsigned int rgb_to_pixel32bgr_dup(unsigned int r, unsigned int g,
                                                 unsigned int b)
{
    unsigned int col;
    col = rgb_to_pixel32bgr(r, g, b);
    return col;
}
