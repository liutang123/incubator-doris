// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "olap/rowset/segment_v2/column_reader.h"
#include "olap/rowset/segment_v2/column_writer.h"
#include "olap/tablet_schema_helper.h"
#include "olap/decimal12.h"

#include <gtest/gtest.h>
#include <iostream>

#include "common/logging.h"
#include "env/env.h"
#include "olap/column_block.h"
#include "olap/fs/fs_util.h"
#include "olap/olap_common.h"
#include "olap/types.h"
#include "runtime/mem_tracker.h"
#include "runtime/mem_pool.h"
#include "util/file_utils.h"

using std::string;

namespace doris {
namespace segment_v2 {

static const string TEST_DIR = "./ut_dir/column_reader_writer_test";

class ColumnReaderWriterTest : public testing::Test {
public:
    ColumnReaderWriterTest() : _tracker(new MemTracker()), _pool(_tracker.get()) {}
    virtual ~ColumnReaderWriterTest() {}

protected:
    void SetUp() override {
        if (FileUtils::check_exist(TEST_DIR)) {
            ASSERT_TRUE(FileUtils::remove_all(TEST_DIR).ok());
        }
        ASSERT_TRUE(FileUtils::create_dir(TEST_DIR).ok());
    }

    void TearDown() override {
        if (FileUtils::check_exist(TEST_DIR)) {
            ASSERT_TRUE(FileUtils::remove_all(TEST_DIR).ok());
        }
    }

private:
    std::shared_ptr<MemTracker> _tracker;
    MemPool _pool;
};

template<FieldType type, EncodingTypePB encoding>
void test_nullable_data(uint8_t* src_data, uint8_t* src_is_null, int num_rows, string test_name) {
    using Type = typename TypeTraits<type>::CppType;
    Type* src = (Type*)src_data;

    ColumnMetaPB meta;

    // write data
    string fname = TEST_DIR + "/" + test_name;
    {
        std::unique_ptr<fs::WritableBlock> wblock;
        fs::CreateBlockOptions opts({ fname });
        Status st = fs::fs_util::block_mgr_for_ut()->create_block(opts, &wblock);
        ASSERT_TRUE(st.ok()) << st.get_error_msg();

        ColumnWriterOptions writer_opts;
        writer_opts.meta = &meta;
        writer_opts.meta->set_column_id(0);
        writer_opts.meta->set_unique_id(0);
        writer_opts.meta->set_type(type);
        if (type == OLAP_FIELD_TYPE_CHAR || type == OLAP_FIELD_TYPE_VARCHAR) {
            writer_opts.meta->set_length(10);
        } else {
            writer_opts.meta->set_length(0);
        }
        writer_opts.meta->set_encoding(encoding);
        writer_opts.meta->set_compression(segment_v2::CompressionTypePB::LZ4F);
        writer_opts.meta->set_is_nullable(true);
        writer_opts.need_zone_map = true;

        TabletColumn column(OLAP_FIELD_AGGREGATION_NONE, type);
        if (type == OLAP_FIELD_TYPE_VARCHAR) {
            column = create_varchar_key(1);
        } else if (type == OLAP_FIELD_TYPE_CHAR) {
            column = create_char_key(1);
        }
        std::unique_ptr<ColumnWriter> writer;
        ColumnWriter::create(writer_opts, &column, wblock.get(), &writer);
        st = writer->init();
        ASSERT_TRUE(st.ok()) << st.to_string();

        for (int i = 0; i < num_rows; ++i) {
            st = writer->append(BitmapTest(src_is_null, i), src + i);
            ASSERT_TRUE(st.ok());
        }

        ASSERT_TRUE(writer->finish().ok());
        ASSERT_TRUE(writer->write_data().ok());
        ASSERT_TRUE(writer->write_ordinal_index().ok());
        ASSERT_TRUE(writer->write_zone_map().ok());

        // close the file
        ASSERT_TRUE(wblock->close().ok());
    }
    const TypeInfo* type_info = get_scalar_type_info(type);
    // read and check
    {
        // read and check
        ColumnReaderOptions reader_opts;
        std::unique_ptr<ColumnReader> reader;
        auto st = ColumnReader::create(reader_opts, meta, num_rows, fname, &reader);
        ASSERT_TRUE(st.ok());

        ColumnIterator* iter = nullptr;
        st = reader->new_iterator(&iter);
        ASSERT_TRUE(st.ok());
        std::unique_ptr<fs::ReadableBlock> rblock;
        fs::BlockManager* block_manager = fs::fs_util::block_mgr_for_ut();
        block_manager->open_block(fname, &rblock);
        
        ASSERT_TRUE(st.ok());
        ColumnIteratorOptions iter_opts;
        OlapReaderStatistics stats;
        iter_opts.stats = &stats;
        iter_opts.rblock = rblock.get();
        st = iter->init(iter_opts);
        ASSERT_TRUE(st.ok());
        // sequence read
        {
            st = iter->seek_to_first();
            ASSERT_TRUE(st.ok()) << st.to_string();

<<<<<<< HEAD
            auto tracker = std::make_shared<MemTracker>();
            MemPool pool(tracker.get());
            Type vals[1024];
            Type* vals_ = vals;
            uint8_t is_null[1024];
            ColumnBlock col(type_info, (uint8_t*)vals, is_null, 1024, &pool);
=======
            MemTracker tracker;
            MemPool pool(&tracker);
            std::unique_ptr<ColumnVectorBatch> cvb;
            ColumnVectorBatch::create(1024, true, type_info, &cvb);
            ColumnBlock col(cvb.get(), &pool);
>>>>>>> Restructure storage type to support list type.

            int idx = 0;
            while (true) {
                size_t rows_read = 1024;
                ColumnBlockView dst(&col);
                st = iter->next_batch(&rows_read, &dst);
                ASSERT_TRUE(st.ok());
                for (int j = 0; j < rows_read; ++j) {
                    ASSERT_EQ(BitmapTest(src_is_null, idx), col.is_null(j));
                    if (!col.is_null(j)) {
                        if (type == OLAP_FIELD_TYPE_VARCHAR || type == OLAP_FIELD_TYPE_CHAR) {
                            Slice* src_slice = (Slice*)src_data;
                            ASSERT_EQ(src_slice[idx].to_string(),
                                    reinterpret_cast<const Slice*>(col.cell_ptr(j))->to_string()) << "j:" << j;
                        } else {
                            ASSERT_EQ(src[idx], *reinterpret_cast<const Type*>(col.cell_ptr(j)));
                        }
                    }
                    idx++;
                }
                if (rows_read < 1024) {
                    break;
                }
            }
        }

        {
<<<<<<< HEAD
            auto tracker = std::make_shared<MemTracker>();
            MemPool pool(tracker.get());
            Type vals[1024];
            uint8_t is_null[1024];
            ColumnBlock col(type_info, (uint8_t*)vals, is_null, 1024, &pool);
=======
            MemTracker tracker;
            MemPool pool(&tracker);
            std::unique_ptr<ColumnVectorBatch> cvb;
            ColumnVectorBatch::create(1024, true, type_info, &cvb);
            ColumnBlock col(cvb.get(), &pool);
>>>>>>> Restructure storage type to support list type.

            for (int rowid = 0; rowid < num_rows; rowid += 4025) {
                st = iter->seek_to_ordinal(rowid);
                ASSERT_TRUE(st.ok());

                int idx = rowid;
                size_t rows_read = 1024;
                ColumnBlockView dst(&col);

                st = iter->next_batch(&rows_read, &dst);
                ASSERT_TRUE(st.ok());
                for (int j = 0; j < rows_read; ++j) {
                    ASSERT_EQ(BitmapTest(src_is_null, idx), col.is_null(j));
                    if (!col.is_null(j)) {
                        if (type == OLAP_FIELD_TYPE_VARCHAR || type == OLAP_FIELD_TYPE_CHAR) {
                            Slice* src_slice = (Slice*)src_data;
                            ASSERT_EQ(src_slice[idx].to_string(),
                                    reinterpret_cast<const Slice*>(col.cell_ptr(j))->to_string());
                        } else {
                            ASSERT_EQ(src[idx], *reinterpret_cast<const Type*>(col.cell_ptr(j)));
                        }
                    }
                    idx++;
                }
            }
        }

        delete iter;
    }
}

template<FieldType item_type, EncodingTypePB item_encoding, EncodingTypePB list_encoding>
void test_list_nullable_data(collection* src_data, uint8_t* src_is_null, int num_rows, string test_name) {
    collection* src = src_data;
    ColumnMetaPB meta;

    // write data
    string fname = TEST_DIR + "/" + test_name;
    {
        std::unique_ptr<WritableFile> wfile;
        auto st = Env::Default()->new_writable_file(fname, &wfile);
        ASSERT_TRUE(st.ok());

        ColumnWriterOptions writer_opts;
        writer_opts.meta = &meta;
        writer_opts.meta->set_column_id(0);
        writer_opts.meta->set_unique_id(0);
        writer_opts.meta->set_type(OLAP_FIELD_TYPE_LIST);
        writer_opts.meta->set_length(0);
        writer_opts.meta->set_encoding(list_encoding);
        writer_opts.meta->set_compression(segment_v2::CompressionTypePB::LZ4F);
        writer_opts.meta->set_is_nullable(true);
        writer_opts.need_zone_map = false;
        writer_opts.data_page_size = 5 * 8;

        TabletColumn list_column(OLAP_FIELD_AGGREGATION_NONE, OLAP_FIELD_TYPE_LIST);
        int32 item_length = 0;
        if (item_type == OLAP_FIELD_TYPE_CHAR || item_type == OLAP_FIELD_TYPE_VARCHAR) {
            item_length = 10;
        }
        TabletColumn item_column(OLAP_FIELD_AGGREGATION_NONE, item_type, true, 0, item_length);
        list_column.add_sub_column(item_column);

        std::unique_ptr<ColumnWriter> writer;
        ColumnWriter::create(writer_opts, &list_column, wfile.get(), &writer);
        st = writer->init();
        ASSERT_TRUE(st.ok()) << st.to_string();

        for (int i = 0; i < num_rows; ++i) {
            st = writer->append(BitmapTest(src_is_null, i), src + i);
            ASSERT_TRUE(st.ok());
        }

        st = writer->finish();
        ASSERT_TRUE(st.ok());

        st = writer->write_data();
        ASSERT_TRUE(st.ok());
        st = writer->write_ordinal_index();
        ASSERT_TRUE(st.ok());

        // close the file
        wfile.reset();
    }
    TypeInfo* type_info = get_type_info(&meta);
    // read and check
    {
        // read and check
        ColumnReaderOptions reader_opts;
        std::unique_ptr<ColumnReader> reader;
        auto st = ColumnReader::create(reader_opts, meta, num_rows, fname, &reader);
        ASSERT_TRUE(st.ok());

        ColumnIterator* iter = nullptr;
        st = reader->new_iterator(&iter);
        ASSERT_TRUE(st.ok());
        std::unique_ptr<RandomAccessFile> rfile;
        st = Env::Default()->new_random_access_file(fname, &rfile);
        ASSERT_TRUE(st.ok());
        ColumnIteratorOptions iter_opts;
        OlapReaderStatistics stats;
        iter_opts.stats = &stats;
        iter_opts.file = rfile.get();
        st = iter->init(iter_opts);
        ASSERT_TRUE(st.ok());
        // sequence read
        {
            st = iter->seek_to_first();
            ASSERT_TRUE(st.ok()) << st.to_string();

            MemTracker tracker;
            MemPool pool(&tracker);
            std::unique_ptr<ColumnVectorBatch> cvb;
            ColumnVectorBatch::create(1024, true, type_info, &cvb);
            ColumnBlock col(cvb.get(), &pool);

            int idx = 0;
            while (true) {
                size_t rows_read = 1024;
                ColumnBlockView dst(&col);
                st = iter->next_batch(&rows_read, &dst);
                ASSERT_TRUE(st.ok());
                for (int j = 0; j < rows_read; ++j) {
                    ASSERT_EQ(BitmapTest(src_is_null, idx), col.is_null(j));
                    if (!col.is_null(j)) {
                        type_info->equal(&src[idx], col.cell_ptr(j));
                    }
                    ++idx;
                }
                if (rows_read < 1024) {
                    break;
                }
            }
        }
        // seek read
        {
            MemTracker tracker;
            MemPool pool(&tracker);
            std::unique_ptr<ColumnVectorBatch> cvb;
            ColumnVectorBatch::create(1024, true, type_info, &cvb);
            ColumnBlock col(cvb.get(), &pool);

            for (int rowid = 0; rowid < num_rows; rowid += 4025) {
                st = iter->seek_to_ordinal(rowid);
                ASSERT_TRUE(st.ok());

                int idx = rowid;
                size_t rows_read = 1024;
                ColumnBlockView dst(&col);

                st = iter->next_batch(&rows_read, &dst);
                ASSERT_TRUE(st.ok());
                for (int j = 0; j < rows_read; ++j) {
                    ASSERT_EQ(BitmapTest(src_is_null, idx), col.is_null(j));
                    if (!col.is_null(j)) {
                        ASSERT_TRUE(type_info->equal(&src[idx], col.cell_ptr(j)));
                    }
                    ++idx;
                }
            }
        }
        delete iter;
    }
}


TEST_F(ColumnReaderWriterTest, test_list_type) {
    size_t num_list = 1024 * 1024;
    size_t num_item = num_list * 3;

    uint8_t* list_is_null = new uint8_t[BitmapSize(num_list)];
    collection* list_val = new collection[num_list];
    bool* item_is_null = new bool[num_item];
    uint8_t* item_val = new uint8_t[num_item];
    for (int i = 0; i < num_item; ++i) {
        // 0[0 1 2]null 1[3 4 5] 2[6 7 8] 3[9 10 11] 4[12 13 14]null 5[15 16 17]
        item_val[i] = i;
        item_is_null[i] = (i % 4) == 0;
        if (i % 3 == 0) {
            size_t list_index = i / 3;
            bool is_null = (list_index % 4) == 1;
            BitmapChange(list_is_null, list_index, is_null);
            if (is_null) {
                continue;
            }
            list_val[list_index].data = &item_val[i];
            list_val[list_index].null_signs = &item_is_null[i];
            list_val[list_index].length = 3;
        }
    }
    test_list_nullable_data<OLAP_FIELD_TYPE_TINYINT, BIT_SHUFFLE, BIT_SHUFFLE>(list_val, list_is_null, num_list, "null_list_bs");

    delete[] list_val;
    list_val = new collection[num_list];


    Slice* varchar_vals = new Slice[3];
    delete[] item_is_null;
    item_is_null = new bool[3];
    for (int i = 0; i < 3; ++i) {
        item_is_null[i] = i == 1;
        if (i != 1) {
            set_column_value_by_type(OLAP_FIELD_TYPE_VARCHAR, i, (char*)&varchar_vals[i], &_pool);
        }
    }
    for (int i = 0; i <  num_list; ++i) {
        bool is_null = (i % 4) == 1;
        BitmapChange(list_is_null, i, is_null);
        if (is_null) {
            continue;
        }
        list_val[i].data = varchar_vals;
        list_val[i].null_signs = item_is_null;
        list_val[i].length = 3;
    }

    delete[] list_val;
    delete[] list_is_null;
    delete[] item_is_null;
    delete[] item_val;
}


template<FieldType type>
void test_read_default_value(string value, void* result) {
    using Type = typename TypeTraits<type>::CppType;
    TypeInfo* type_info = get_type_info(type);
    // read and check
    {
        TabletColumn tablet_column = create_with_default_value<type>(value);
        DefaultValueColumnIterator iter(tablet_column.has_default_value(),
                                        tablet_column.default_value(),
                                        tablet_column.is_nullable(),
                                        type_info,
                                        tablet_column.length());
        ColumnIteratorOptions iter_opts;
        auto st = iter.init(iter_opts);
        ASSERT_TRUE(st.ok());
        // sequence read
        {
            st = iter.seek_to_first();
            ASSERT_TRUE(st.ok()) << st.to_string();

<<<<<<< HEAD
            auto tracker = std::make_shared<MemTracker>();
            MemPool pool(tracker.get());
            Type vals[1024];
            Type* vals_ = vals;
            uint8_t is_null[1024];
            ColumnBlock col(type_info, (uint8_t*)vals, is_null, 1024, &pool);
=======
            MemTracker tracker;
            MemPool pool(&tracker);
            std::unique_ptr<ColumnVectorBatch> cvb;
            ColumnVectorBatch::create(1024, true, type_info, &cvb);
            ColumnBlock col(cvb.get(), &pool);
>>>>>>> Restructure storage type to support list type.

            int idx = 0;
            size_t rows_read = 1024;
            ColumnBlockView dst(&col);
            st = iter.next_batch(&rows_read, &dst);
            ASSERT_TRUE(st.ok());
            for (int j = 0; j < rows_read; ++j) {
                if (type == OLAP_FIELD_TYPE_CHAR) {
                    ASSERT_EQ(*(string*)result, reinterpret_cast<const Slice*>(col.cell_ptr(j))->to_string()) << "j:" << j;
                } else if (type == OLAP_FIELD_TYPE_VARCHAR
                        || type == OLAP_FIELD_TYPE_HLL
                        || type == OLAP_FIELD_TYPE_OBJECT) {
                    ASSERT_EQ(value, reinterpret_cast<const Slice*>(col.cell_ptr(j))->to_string()) << "j:" << j;
                } else {
                    ;
                    ASSERT_EQ(*(Type*)result, *(reinterpret_cast<const Type*>(col.cell_ptr(j))));
                }
                idx++;
            }
        }

        {
<<<<<<< HEAD
            auto tracker = std::make_shared<MemTracker>();
            MemPool pool(tracker.get());
            Type vals[1024];
            uint8_t is_null[1024];
            ColumnBlock col(type_info, (uint8_t*)vals, is_null, 1024, &pool);
=======
            MemTracker tracker;
            MemPool pool(&tracker);
            std::unique_ptr<ColumnVectorBatch> cvb;
            ColumnVectorBatch::create(1024, true, type_info, &cvb);
            ColumnBlock col(cvb.get(), &pool);
>>>>>>> Restructure storage type to support list type.

            for (int rowid = 0; rowid < 2048; rowid += 128) {
                st = iter.seek_to_ordinal(rowid);
                ASSERT_TRUE(st.ok());

                int idx = rowid;
                size_t rows_read = 1024;
                ColumnBlockView dst(&col);
                st = iter.next_batch(&rows_read, &dst);
                ASSERT_TRUE(st.ok());
                for (int j = 0; j < rows_read; ++j) {
                    if (type == OLAP_FIELD_TYPE_CHAR) {
                        ASSERT_EQ(*(string*)result, reinterpret_cast<const Slice*>(col.cell_ptr(j))->to_string()) << "j:" << j;
                    } else if (type == OLAP_FIELD_TYPE_VARCHAR
                            || type == OLAP_FIELD_TYPE_HLL
                            || type == OLAP_FIELD_TYPE_OBJECT) {
                        ASSERT_EQ(value, reinterpret_cast<const Slice*>(col.cell_ptr(j))->to_string());
                    } else {
                        ASSERT_EQ(*(Type*)result, *(reinterpret_cast<const Type*>(col.cell_ptr(j))));
                    }
                    idx++;
                }
            }
        }
    }
}

TEST_F(ColumnReaderWriterTest, test_nullable) {
    size_t num_uint8_rows = 1024 * 1024;
    uint8_t* is_null = new uint8_t[num_uint8_rows];
    uint8_t* val = new uint8_t[num_uint8_rows];
    for (int i = 0; i < num_uint8_rows; ++i) {
        val[i] = i;
        BitmapChange(is_null, i, (i % 4) == 0);
    }

    test_nullable_data<OLAP_FIELD_TYPE_TINYINT, BIT_SHUFFLE>(val, is_null, num_uint8_rows, "null_tiny_bs");
    test_nullable_data<OLAP_FIELD_TYPE_SMALLINT, BIT_SHUFFLE>(val, is_null, num_uint8_rows / 2, "null_smallint_bs");
    test_nullable_data<OLAP_FIELD_TYPE_INT, BIT_SHUFFLE>(val, is_null, num_uint8_rows / 4, "null_int_bs");
    test_nullable_data<OLAP_FIELD_TYPE_BIGINT, BIT_SHUFFLE>(val, is_null, num_uint8_rows / 8, "null_bigint_bs");
    test_nullable_data<OLAP_FIELD_TYPE_LARGEINT, BIT_SHUFFLE>(val, is_null, num_uint8_rows / 16, "null_largeint_bs");

    // test for the case where most values are not null
    uint8_t* is_null_sparse = new uint8_t[num_uint8_rows];
    for (int i = 0; i < num_uint8_rows; ++i) {
        bool v = false;
        // in order to make some data pages not null, set the first half of values not null.
        // for the second half, only 1/1024 of values are null
        if (i >= (num_uint8_rows / 2)) {
            v = (i % 1024) == 10;
        }
        BitmapChange(is_null_sparse, i, v);
    }
    test_nullable_data<OLAP_FIELD_TYPE_TINYINT, BIT_SHUFFLE>(val, is_null_sparse, num_uint8_rows, "sparse_null_tiny_bs");


    float* float_vals = new float[num_uint8_rows];
    for (int i = 0; i < num_uint8_rows; ++i) {
        float_vals[i] = i;
        is_null[i] = ((i % 16) == 0);
    }
    test_nullable_data<OLAP_FIELD_TYPE_FLOAT, BIT_SHUFFLE>((uint8_t*)float_vals, is_null, num_uint8_rows, "null_float_bs");

    double* double_vals = new double[num_uint8_rows];
    for (int i = 0; i < num_uint8_rows; ++i) {
        double_vals[i] = i;
        is_null[i] = ((i % 16) == 0);
    }
    test_nullable_data<OLAP_FIELD_TYPE_DOUBLE, BIT_SHUFFLE>((uint8_t*)double_vals, is_null, num_uint8_rows, "null_double_bs");
    // test_nullable_data<OLAP_FIELD_TYPE_FLOAT, BIT_SHUFFLE>(val, is_null, num_uint8_rows / 4, "null_float_bs");
    // test_nullable_data<OLAP_FIELD_TYPE_DOUBLE, BIT_SHUFFLE>(val, is_null, num_uint8_rows / 8, "null_double_bs");
    delete[] val;
    delete[] is_null;
    delete[] is_null_sparse;
    delete[] float_vals;
    delete[] double_vals;
}

TEST_F(ColumnReaderWriterTest, test_types) {
    size_t num_uint8_rows = 1024 * 1024;
    uint8_t* is_null = new uint8_t[num_uint8_rows];

    bool* bool_vals = new bool[num_uint8_rows];
    uint24_t* date_vals = new uint24_t[num_uint8_rows];
    uint64_t* datetime_vals = new uint64_t[num_uint8_rows];
    decimal12_t* decimal_vals = new decimal12_t[num_uint8_rows];
    Slice* varchar_vals = new Slice[num_uint8_rows];
    Slice* char_vals = new Slice[num_uint8_rows];
    for (int i = 0; i < num_uint8_rows; ++i) {
        bool_vals[i] = i % 2;
        date_vals[i] = i + 33;
        datetime_vals[i] = i + 33;
        decimal_vals[i] = decimal12_t(i, i); // 1.000000001

        set_column_value_by_type(OLAP_FIELD_TYPE_VARCHAR, i, (char*)&varchar_vals[i], &_pool);
        set_column_value_by_type(OLAP_FIELD_TYPE_CHAR, i, (char*)&char_vals[i], &_pool, 8);

        BitmapChange(is_null, i, (i % 4) == 0);
    }
    test_nullable_data<OLAP_FIELD_TYPE_CHAR, DICT_ENCODING>((uint8_t*)char_vals, is_null, num_uint8_rows, "null_char_bs");
    test_nullable_data<OLAP_FIELD_TYPE_VARCHAR, DICT_ENCODING>((uint8_t*)varchar_vals, is_null, num_uint8_rows, "null_varchar_bs");
    test_nullable_data<OLAP_FIELD_TYPE_BOOL, BIT_SHUFFLE>((uint8_t*)bool_vals, is_null, num_uint8_rows, "null_bool_bs");
    test_nullable_data<OLAP_FIELD_TYPE_DATE, BIT_SHUFFLE>((uint8_t*)date_vals, is_null, num_uint8_rows / 3, "null_date_bs");

    for (int i = 0; i < num_uint8_rows; ++i) {
        BitmapChange(is_null, i, (i % 16) == 0);
    }
    test_nullable_data<OLAP_FIELD_TYPE_DATETIME, BIT_SHUFFLE>((uint8_t*)datetime_vals, is_null, num_uint8_rows / 8, "null_datetime_bs");

    for (int i = 0; i< num_uint8_rows; ++i) {
        BitmapChange(is_null, i, (i % 24) == 0);
    }
    test_nullable_data<OLAP_FIELD_TYPE_DECIMAL, BIT_SHUFFLE>((uint8_t*)decimal_vals, is_null, num_uint8_rows / 12, "null_decimal_bs");

    delete[] char_vals;
    delete[] varchar_vals;
    delete[] is_null;
    delete[] bool_vals;
    delete[] date_vals;
    delete[] datetime_vals;
    delete[] decimal_vals;
}

TEST_F(ColumnReaderWriterTest, test_default_value) {
    string v_int("1");
    int32_t result = 1;
    test_read_default_value<OLAP_FIELD_TYPE_TINYINT>(v_int, &result);
    test_read_default_value<OLAP_FIELD_TYPE_SMALLINT>(v_int, &result);
    test_read_default_value<OLAP_FIELD_TYPE_INT>(v_int, &result);

    string v_bigint("9223372036854775807");
    int64_t result_bigint =  std::numeric_limits<int64_t>::max();
    test_read_default_value<OLAP_FIELD_TYPE_BIGINT>(v_bigint, &result_bigint);
    int128_t result_largeint =  std::numeric_limits<int64_t>::max();
    test_read_default_value<OLAP_FIELD_TYPE_LARGEINT>(v_bigint, &result_largeint);

    string v_float("1.00");
    float result2 = 1.00;
    test_read_default_value<OLAP_FIELD_TYPE_FLOAT>(v_float, &result2);

    string v_double("1.00");
    double result3 = 1.00;
    test_read_default_value<OLAP_FIELD_TYPE_DOUBLE>(v_double, &result3);

    string v_varchar("varchar");
    test_read_default_value<OLAP_FIELD_TYPE_VARCHAR>(v_varchar, &v_varchar);

    string v_char("char");
    test_read_default_value<OLAP_FIELD_TYPE_CHAR>(v_char, &v_char);

    char* c = (char *)malloc(1);
    c[0] = 0;
    string v_object(c, 1);
    test_read_default_value<OLAP_FIELD_TYPE_HLL>(v_object, &v_object);
    test_read_default_value<OLAP_FIELD_TYPE_OBJECT>(v_object, &v_object);
    free(c);

    string v_date("2019-11-12");
    uint24_t result_date(1034092);
    test_read_default_value<OLAP_FIELD_TYPE_DATE>(v_date, &result_date);

    string v_datetime("2019-11-12 12:01:08");
    int64_t result_datetime = 20191112120108;
    test_read_default_value<OLAP_FIELD_TYPE_DATETIME>(v_datetime, &result_datetime);

    string v_decimal("102418.000000002");
    decimal12_t decimal(102418, 2);
    test_read_default_value<OLAP_FIELD_TYPE_DECIMAL>(v_decimal, &decimal);
}

} // namespace segment_v2
} // namespace doris

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

