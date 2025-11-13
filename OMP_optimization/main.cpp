#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <algorithm>
#include <omp.h>

#define STR_I(x) #x
#define STR(x) STR_I(x)

#ifdef SCHEDULE_CHUNK_SIZE
    #define DEFAULT_CHUNK_SIZE SCHEDULE_CHUNK_SIZE
#else
    #define DEFAULT_CHUNK_SIZE 4096
#endif


struct Arguments {
    std::string input_file;
    std::string output_file;
    float coef;
    bool no_omp;
    int threads;
    bool is_threads_default;
    int chunk_size;
    bool is_static_schedule;
    bool error;
};

struct Image {
    uint32_t width;
    uint32_t height;
    int channels;
    bool is_pgm;
    std::vector<uint8_t> data; 
};

struct BoundaryPixels {
    uint8_t low;
    uint8_t high;
};


Arguments Parser(int argc, char** argv) {
    Arguments result;
    result.input_file = "";
    result.output_file = "";
    result.coef = 0.0f;
    result.no_omp = false;
    result.threads = 0;
    result.is_threads_default = true;
    result.chunk_size = DEFAULT_CHUNK_SIZE;
    result.is_static_schedule = false;
    #ifdef SCHEDULE_KIND
        result.is_static_schedule = (std::strcmp(STR(SCHEDULE_KIND), "static") == 0);
    #endif
    result.error = false;

    for (int i = 1; i < argc; ++i) {
        char* arg = argv[i];
    
        if (!std::strcmp(arg, "--input")) {
            result.input_file = argv[++i];
        } else if (!std::strcmp(arg, "--output")) {
            result.output_file = argv[++i];
         } else if (!std::strcmp(arg, "--coef")) {
            result.coef = std::stof(argv[++i]);
        } else if (!std::strcmp(arg, "--no-omp")) {
            result.no_omp = true;
         } else if (!std::strcmp(arg, "--omp-threads")) {
            result.no_omp = false;
            if (i + 1 < argc && std::isdigit(argv[i + 1][0])) {
                result.threads = std::stoi(argv[++i]);
                result.is_threads_default = false;
            } else {
                ++i;
            }
        } else if (!std::strcmp(arg, "--schedule")) {
            result.is_static_schedule = (!std::strcmp(argv[++i], "static"));
        } else if (!std::strcmp(arg, "--chunk_size")) {
            result.chunk_size = std::stoi(argv[++i]);
            if (result.chunk_size == 0) {
                result.chunk_size = DEFAULT_CHUNK_SIZE;
            }
        } else {
            result.error = true;
            return result;
        }
    }

    if (result.input_file.empty() || result.output_file.empty()) {
        result.error = true;
    }

    return result;
}


Image ReadFile(std::string& input_file) {
    Image image;
    std::ifstream file(input_file, std::ios::binary);
    std::string type;
    file >> type;
    image.is_pgm = (type == "P5");
    image.channels = image.is_pgm ? 1 : 3;
    int max_pixel;
    file >> image.width >> image.height >> max_pixel;
    file.get();
    std::streamsize size = (uint64_t)image.width * (uint64_t)image.height * (uint64_t)image.channels;
    image.data.resize(size);
    file.read(reinterpret_cast<char*>(image.data.data()), size);
    return image;
}


void WriteFile(std::string& output_file, Image& image) {
    uint64_t size = image.data.size();

    std::ofstream file(output_file, std::ios::binary);
    if (image.is_pgm) {
        file << "P5\n";
    } else {
        file << "P6\n";
    }
    file << image.width << ' ' << image.height << "\n255\n";

    std::streamsize s_size = static_cast<std::streamsize>(size);
    file.write(reinterpret_cast<char*>(image.data.data()),s_size);
}


void PgmHistogramCreating(Image& image, uint32_t histogram[256]) {
    for (int i = 0; i < 256; ++i) {
        histogram[i] = 0;
    }

    uint8_t* data = image.data.data();
    int64_t size = image.data.size();

    for (int64_t i = 0; i < size; ++i) {
        ++histogram[data[i]];
    }
}

void PpmHistogramCreating(Image& image, uint32_t r_histogram[256],  uint32_t g_histogram[256], uint32_t b_histogram[256]) {
    for (int i = 0; i < 256; ++i) {
        r_histogram[i] = 0;
        g_histogram[i] = 0;
        b_histogram[i] = 0;
    }

    uint8_t* data = image.data.data();
    int64_t size = image.data.size();

    for (int64_t i = 0; i < size; i += 3) { 
        ++r_histogram[data[i]];
        ++g_histogram[data[i + 1]];
        ++b_histogram[data[i + 2]];
    }
}

void OmpPgmHistogramCreating(Image& image, uint32_t histogram[256]) {
    for (int i = 0; i < 256; ++i) {
        histogram[i] = 0;
    }

    uint8_t* data = image.data.data();
    int64_t size = image.data.size();

    #pragma omp parallel
    {
        uint32_t small_histogram[256];
        for (int i = 0; i < 256; ++i) {
            small_histogram[i] = 0;
        }

        #pragma omp for schedule(runtime)
        for (int64_t i = 0; i < size; ++i) {
            ++small_histogram[data[i]];
        }

        #pragma omp critical
        {
            for (int i = 0; i < 256; ++i) {
                histogram[i] += small_histogram[i];
            }
        } 
    }
}

void OmpPpmHistogramCreating(Image& image, uint32_t r_histogram[256],  uint32_t g_histogram[256], uint32_t b_histogram[256]) {
    for (int i = 0; i < 256; ++i) {
        r_histogram[i] = 0;
        g_histogram[i] = 0;
        b_histogram[i] = 0;
    }

    uint8_t* data = image.data.data();
    int64_t size = image.data.size();

    #pragma omp parallel
    {
        uint32_t small_r_histogram[256];
        uint32_t small_g_histogram[256];
        uint32_t small_b_histogram[256];
        for (int i = 0; i < 256; ++i) {
            small_r_histogram[i] = 0;
            small_g_histogram[i] = 0;
            small_b_histogram[i] = 0;
        }

        #pragma omp for schedule(runtime)
        for (int64_t i = 0; i < size; i += 3) {
            ++small_r_histogram[data[i]];
            ++small_g_histogram[data[i + 1]];
            ++small_b_histogram[data[i + 2]];
        }

        #pragma omp critical
        {
            for (int i = 0; i < 256; ++i) {
                r_histogram[i] += small_r_histogram[i];
                g_histogram[i] += small_g_histogram[i];
                b_histogram[i] += small_b_histogram[i];
            }
        } 
    }
}


BoundaryPixels BoundaryFind(uint32_t ignored_pixels, const uint32_t histogram[256]) {
    BoundaryPixels boundary_pixels;
    uint32_t lower_sum = 0;
    uint32_t upper_sum = 0;

    for (int i = 0; i < 256; ++i) {
        lower_sum += histogram[i];

        if (lower_sum > ignored_pixels) {
            boundary_pixels.low = i;
            break;
        }
    }
    for (int i = 255; i >= 0; --i) {
        upper_sum += histogram[i];

        if (upper_sum > ignored_pixels) {
            boundary_pixels.high = i;
            return boundary_pixels;
        }
    }
    return boundary_pixels;
}

BoundaryPixels PpmBoundaryFind(uint32_t ignored_pixels, const uint32_t r_histogram[256], const uint32_t g_histogram[256], const uint32_t b_histogram[256]) {
    BoundaryPixels r_boundary_pixels = BoundaryFind(ignored_pixels, r_histogram);
    BoundaryPixels g_boundary_pixels = BoundaryFind(ignored_pixels, g_histogram);
    BoundaryPixels b_boundary_pixels = BoundaryFind(ignored_pixels, b_histogram);
    BoundaryPixels final_boundary_pixels;
    final_boundary_pixels.low = std::min(r_boundary_pixels.low, std::min(g_boundary_pixels.low, b_boundary_pixels.low));
    final_boundary_pixels.high = std::max(r_boundary_pixels.high, std::max(g_boundary_pixels.high, b_boundary_pixels.high));
    return final_boundary_pixels;
}


void MappingTableCreating(uint8_t mapping_table[256], BoundaryPixels boundary_pixels) {
    for (int i = 0; i <= boundary_pixels.low; ++i) {
        mapping_table[i] = 0;
    }
    for (int i = 255; i >= boundary_pixels.high; --i) {
        mapping_table[i] = 255;
    }
    float mapping_coef = 255.0 / float(boundary_pixels.high - boundary_pixels.low);
    for (int i = boundary_pixels.low + 1; i < boundary_pixels.high; ++i) {
        mapping_table[i] = uint8_t((i - boundary_pixels.low) * mapping_coef + 0.5);
    }
}


void OmpNewImageCreating(Image& image, const uint8_t mapping_table[256]) {
    uint8_t* data = image.data.data();
    const int64_t size = image.data.size();
    #pragma omp parallel 
    {
        #pragma omp for schedule(runtime)
        for (int64_t i = 0; i < size; ++i) {
            data[i] = mapping_table[data[i]];
        }
    }
}

void NewImageCreating(Image& image, const uint8_t mapping_table[256]) {
    uint8_t* data = image.data.data();
    const int64_t size = image.data.size();
    for (int64_t i = 0; i < size; ++i) {
        data[i] = mapping_table[data[i]];
    }
}


int main(int argc, char** argv) {
    Arguments args = Parser(argc, argv);
    if (args.error)  {
        std::cerr << "Input data error";
        return 1;
    }
    
    Image image = ReadFile(args.input_file);

    if (args.is_threads_default && !args.no_omp) {
        if (image.width * image.height < 500000) {
            args.threads = std::min(4, omp_get_num_procs());
        } else if (image.width * image.height < 1500000) {
            args.threads = std::min(8, omp_get_num_procs());
        } else {
            args.threads = omp_get_num_procs();
        }
    }

    if (!args.no_omp) {
        if (args.threads != 0) {
            omp_set_num_threads(args.threads);
        }
        if (args.is_static_schedule) {
            omp_set_schedule(omp_sched_static, args.chunk_size);
        } else {
            omp_set_schedule(omp_sched_dynamic, args.chunk_size);
        }
    }

    double start = omp_get_wtime();

    uint64_t number_of_pixels = (uint64_t)image.width * (uint64_t)image.height;
    uint32_t ignored_pixels = (uint32_t)((float)number_of_pixels * args.coef);
    BoundaryPixels boundary_pixels;

    if (image.is_pgm) {
        uint32_t histogram[256];
        if (args.no_omp) {
            PgmHistogramCreating(image, histogram);
        } else {
            OmpPgmHistogramCreating(image, histogram);
        }
        boundary_pixels = BoundaryFind(ignored_pixels, histogram);
    } else {
        uint32_t r_histogram[256];
        uint32_t g_histogram[256];
        uint32_t b_histogram[256];
        if (args.no_omp) {
            PpmHistogramCreating(image, r_histogram, g_histogram, b_histogram);
        } else {
            OmpPpmHistogramCreating(image, r_histogram, g_histogram, b_histogram);
        }
        boundary_pixels = PpmBoundaryFind(ignored_pixels, r_histogram, g_histogram, b_histogram);
    }
    uint8_t mapping_table[256];
    if (boundary_pixels.high == boundary_pixels.low) {
        for (int i = 0; i < 256; ++i) {
            mapping_table[i] = i;
        }
    } else {
        MappingTableCreating(mapping_table, boundary_pixels);
    }

    if (args.no_omp) {
        NewImageCreating(image, mapping_table);
    } else {
        OmpNewImageCreating(image, mapping_table);
    }

    double finish = omp_get_wtime();

    int threads = 1;
    if (!args.no_omp) {
        #pragma omp parallel
        {
            if (omp_get_thread_num() == 0) {
                threads = omp_get_num_threads();
            }
        }
    }
    std::printf("Time (%i threads): %lg\n", threads, (finish - start) * 1000.0);


    WriteFile(args.output_file, image);
    return 0;
}