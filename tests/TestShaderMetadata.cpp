#include "candlewick/core/Shader.h"
#include <gtest/gtest.h>

using namespace candlewick;

GTEST_TEST(TestShaderMetadata, uniform_buffer_only) {
  setShadersDirectory(CANDLEWICK_TEST_DATA_DIR);
  auto config = loadShaderMetadata("one_ubuffer.vert");
  EXPECT_EQ(config.uniform_buffers, 1u);
  EXPECT_EQ(config.samplers, 0u);
  EXPECT_EQ(config.storage_textures, 0u);
  EXPECT_EQ(config.storage_buffers, 0u);
}

GTEST_TEST(TestShaderMetadata, no_resources) {
  setShadersDirectory(CANDLEWICK_TEST_DATA_DIR);
  auto config = loadShaderMetadata("no_resources.frag");
  EXPECT_EQ(config.uniform_buffers, 0u);
  EXPECT_EQ(config.samplers, 0u);
  EXPECT_EQ(config.storage_textures, 0u);
  EXPECT_EQ(config.storage_buffers, 0u);
}

GTEST_TEST(TestShaderMetadata, samplers_and_ubuffer) {
  setShadersDirectory(CANDLEWICK_TEST_DATA_DIR);
  auto config = loadShaderMetadata("samplers_and_ubuffer.frag");
  EXPECT_EQ(config.uniform_buffers, 1u);
  EXPECT_EQ(config.samplers, 2u);
  EXPECT_EQ(config.storage_textures, 0u);
  EXPECT_EQ(config.storage_buffers, 0u);
}
