/*
 Tests of custom Google Test assertions.

 Copyright (c) 2012-2014, Victor Zverovich
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gtest-extra.h"

#include <algorithm>
#include <stdexcept>
#include <gtest/gtest-spi.h>

namespace {

std::string FormatSystemErrorMessage(int error_code, fmt::StringRef message) {
  fmt::Writer out;
  fmt::internal::FormatSystemErrorMessage(out, error_code, message);
  return str(out);
}

#define EXPECT_SYSTEM_ERROR(statement, error_code, message) \
  EXPECT_THROW_MSG(statement, fmt::SystemError, \
      FormatSystemErrorMessage(error_code, message))

// Tests that assertion macros evaluate their arguments exactly once.
class SingleEvaluationTest : public ::testing::Test {
 protected:
  SingleEvaluationTest() {
    a_ = 0;
  }
  
  static int a_;
};

int SingleEvaluationTest::a_;

void ThrowNothing() {}

void ThrowException() {
  throw std::runtime_error("test");
}

// Tests that assertion arguments are evaluated exactly once.
TEST_F(SingleEvaluationTest, ExceptionTests) {
  // successful EXPECT_THROW_MSG
  EXPECT_THROW_MSG({  // NOLINT
    a_++;
    ThrowException();
  }, std::exception, "test");
  EXPECT_EQ(1, a_);

  // failed EXPECT_THROW_MSG, throws different type
  EXPECT_NONFATAL_FAILURE(EXPECT_THROW_MSG({  // NOLINT
    a_++;
    ThrowException();
  }, std::logic_error, "test"), "throws a different type");
  EXPECT_EQ(2, a_);

  // failed EXPECT_THROW_MSG, throws an exception with different message
  EXPECT_NONFATAL_FAILURE(EXPECT_THROW_MSG({  // NOLINT
    a_++;
    ThrowException();
  }, std::exception, "other"), "throws an exception with a different message");
  EXPECT_EQ(3, a_);

  // failed EXPECT_THROW_MSG, throws nothing
  EXPECT_NONFATAL_FAILURE(
      EXPECT_THROW_MSG(a_++, std::exception, "test"), "throws nothing");
  EXPECT_EQ(4, a_);
}

// Tests that the compiler will not complain about unreachable code in the
// EXPECT_THROW_MSG macro.
TEST(ExpectThrowTest, DoesNotGenerateUnreachableCodeWarning) {
  int n = 0;
  using std::runtime_error;
  EXPECT_THROW_MSG(throw runtime_error(""), runtime_error, "");
  EXPECT_NONFATAL_FAILURE(EXPECT_THROW_MSG(n++, runtime_error, ""), "");
  EXPECT_NONFATAL_FAILURE(EXPECT_THROW_MSG(throw 1, runtime_error, ""), "");
  EXPECT_NONFATAL_FAILURE(EXPECT_THROW_MSG(
      throw runtime_error("a"), runtime_error, "b"), "");
}

TEST(AssertionSyntaxTest, ExceptionAssertionsBehavesLikeSingleStatement) {
  if (::testing::internal::AlwaysFalse())
    EXPECT_THROW_MSG(ThrowNothing(), std::exception, "");

  if (::testing::internal::AlwaysTrue())
    EXPECT_THROW_MSG(ThrowException(), std::exception, "test");
  else
    ;  // NOLINT
}

// Tests EXPECT_THROW_MSG.
TEST(ExpectTest, EXPECT_THROW_MSG) {
  EXPECT_THROW_MSG(ThrowException(), std::exception, "test");
  EXPECT_NONFATAL_FAILURE(
      EXPECT_THROW_MSG(ThrowException(), std::logic_error, "test"),
      "Expected: ThrowException() throws an exception of "
      "type std::logic_error.\n  Actual: it throws a different type.");
  EXPECT_NONFATAL_FAILURE(
      EXPECT_THROW_MSG(ThrowNothing(), std::exception, "test"),
      "Expected: ThrowNothing() throws an exception of type std::exception.\n"
      "  Actual: it throws nothing.");
  EXPECT_NONFATAL_FAILURE(
      EXPECT_THROW_MSG(ThrowException(), std::exception, "other"),
      "ThrowException() throws an exception with a different message.\n"
      "Expected: other\n"
      "  Actual: test");
}

TEST(StreamingAssertionsTest, ThrowMsg) {
  EXPECT_THROW_MSG(ThrowException(), std::exception, "test")
      << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(
      EXPECT_THROW_MSG(ThrowException(), std::exception, "other")
      << "expected failure", "expected failure");
}

#if FMT_USE_FILE_DESCRIPTORS

TEST(ErrorCodeTest, Ctor) {
  EXPECT_EQ(0, ErrorCode().get());
  EXPECT_EQ(42, ErrorCode(42).get());
}

TEST(FileTest, DefaultCtor) {
  File f;
  EXPECT_EQ(-1, f.get());
}

// Checks if the file is open by reading one character from it.
bool IsOpen(int fd) {
  char buffer;
  return read(fd, &buffer, 1) == 1;
}

bool IsClosed(int fd) {
  char buffer;
  std::streamsize result = read(fd, &buffer, 1);
  return result == -1 && errno == EBADF;
}

TEST(FileTest, OpenFileInCtor) {
  int fd = 0;
  {
    File f(".travis.yml", File::RDONLY);
    fd = f.get();
    ASSERT_TRUE(IsOpen(fd));
  }
  ASSERT_TRUE(IsClosed(fd));
}

TEST(FileTest, OpenFileError) {
  EXPECT_SYSTEM_ERROR(File("nonexistent", File::RDONLY),
      ENOENT, "cannot open file nonexistent");
}

TEST(FileTest, MoveCtor) {
  File f(".travis.yml", File::RDONLY);
  int fd = f.get();
  EXPECT_NE(-1, fd);
  File f2(std::move(f));
  EXPECT_EQ(fd, f2.get());
  EXPECT_EQ(-1, f.get());
}

TEST(FileTest, MoveAssignment) {
  File f(".travis.yml", File::RDONLY);
  int fd = f.get();
  EXPECT_NE(-1, fd);
  File f2;
  f2 = std::move(f);
  EXPECT_EQ(fd, f2.get());
  EXPECT_EQ(-1, f.get());
}

TEST(FileTest, MoveAssignmentClosesFile) {
  File f(".travis.yml", File::RDONLY);
  File f2("CMakeLists.txt", File::RDONLY);
  int old_fd = f2.get();
  f2 = std::move(f);
  EXPECT_TRUE(IsClosed(old_fd));
}

File OpenFile(int &fd) {
  File f(".travis.yml", File::RDONLY);
  fd = f.get();
  return std::move(f);
}

TEST(FileTest, MoveFromTemporaryInCtor) {
  int fd = 0xdeadbeef;
  File f(OpenFile(fd));
  EXPECT_EQ(fd, f.get());
}

TEST(FileTest, MoveFromTemporaryInAssignment) {
  int fd = 0xdeadbeef;
  File f;
  f = OpenFile(fd);
  EXPECT_EQ(fd, f.get());
}

TEST(FileTest, MoveFromTemporaryInAssignmentClosesFile) {
  int fd = 0xdeadbeef;
  File f(".travis.yml", File::RDONLY);
  int old_fd = f.get();
  f = OpenFile(fd);
  EXPECT_TRUE(IsClosed(old_fd));
}

TEST(FileTest, CloseFileInDtor) {
  int fd = 0;
  {
    File f(".travis.yml", File::RDONLY);
    fd = f.get();
  }
  FILE *f = fdopen(fd, "r");
  int error_code = errno;
  if (f)
    fclose(f);
  EXPECT_TRUE(f == 0);
  EXPECT_EQ(EBADF, error_code);
}

TEST(FileTest, CloseError) {
  File *fd = new File(".travis.yml", File::RDONLY);
  EXPECT_STDERR(close(fd->get()); delete fd,
    FormatSystemErrorMessage(EBADF, "cannot close file") + "\n");
}

std::string ReadLine(File &f) {
  enum { BUFFER_SIZE = 100 };
  char buffer[BUFFER_SIZE];
  std::streamsize result = f.read(buffer, BUFFER_SIZE);
  buffer[std::min<std::streamsize>(BUFFER_SIZE - 1, result)] = '\0';
  if (char *end = strchr(buffer, '\n'))
    *end = '\0';
  return buffer;
}

TEST(FileTest, Read) {
  File f(".travis.yml", File::RDONLY);
  EXPECT_EQ("language: cpp", ReadLine(f));
}

TEST(FileTest, ReadError) {
  File f;
  char buf;
  EXPECT_SYSTEM_ERROR(f.read(&buf, 1), EBADF, "cannot read from file");
}

TEST(FileTest, Dup) {
  File f(".travis.yml", File::RDONLY);
  File dup = File::dup(f.get());
  EXPECT_NE(f.get(), dup.get());
  EXPECT_EQ("language: cpp", ReadLine(dup));
}

TEST(FileTest, DupError) {
  EXPECT_SYSTEM_ERROR(File::dup(-1),
      EBADF, "cannot duplicate file descriptor -1");
}

TEST(FileTest, Dup2) {
  File f(".travis.yml", File::RDONLY);
  File dup("CMakeLists.txt", File::RDONLY);
  f.dup2(dup.get());
  EXPECT_NE(f.get(), dup.get());
  EXPECT_EQ("language: cpp", ReadLine(dup));
}

TEST(FileTest, Dup2Error) {
  File f(".travis.yml", File::RDONLY);
  EXPECT_SYSTEM_ERROR(f.dup2(-1), EBADF,
      fmt::Format("cannot duplicate file descriptor {} to -1") << f.get());

}

TEST(FileTest, Dup2NoExcept) {
  File f(".travis.yml", File::RDONLY);
  File dup("CMakeLists.txt", File::RDONLY);
  ErrorCode ec;
  f.dup2(dup.get(), ec);
  EXPECT_EQ(0, ec.get());
  EXPECT_NE(f.get(), dup.get());
  EXPECT_EQ("language: cpp", ReadLine(dup));
}

TEST(FileTest, Dup2NoExceptError) {
  File f(".travis.yml", File::RDONLY);
  ErrorCode ec;
  f.dup2(-1, ec);
  EXPECT_EQ(EBADF, ec.get());
}

TEST(FileTest, Pipe) {
  File read_end, write_end;
  File::pipe(read_end, write_end);
  EXPECT_NE(-1, read_end.get());
  EXPECT_NE(-1, write_end.get());
  // TODO: try writing to write_fd and reading from read_fd
}

// TODO: test pipe

// TODO: test File::read

// TODO: compile both with C++11 & C++98 mode

#endif

// TODO: test OutputRedirector

}  // namespace