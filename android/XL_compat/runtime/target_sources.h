#ifndef DXX_XL_TARGET_SOURCES_H
#define DXX_XL_TARGET_SOURCES_H
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
struct mission_preparation;
namespace xl_compat
{
// Synchronous read-only view; the caller retains the descriptor for its lifetime
class TargetSources
{
  public:
	TargetSources(mission_preparation *mission, std::string installation);
	~TargetSources();
	const std::string &archive() const;
	int ordinal(const char *level, int requested) const;
	bool archive_contains(const std::string &name);
	std::optional<std::vector<unsigned char>> archive_bytes(const std::string &name, std::size_t limit);
	std::optional<std::vector<unsigned char>> directory_bytes(const std::string &name, std::size_t limit, bool installation = false);
	bool directory_exists(const std::string &name);

  private:
	struct State;
	std::unique_ptr<State> state;
};
} // namespace xl_compat
#endif
