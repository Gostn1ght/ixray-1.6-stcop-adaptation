// xrOGFConverter: turns compiled game models (.ogf) back into SDK library objects (.object)
// so props of the game can be placed as static objects in the Level Editor.
//
//   xrOGFConverter <model.ogf | folder> <output folder> [-skeletal] [-static] [-mu]
//
// A folder is converted recursively and the relative layout is kept.
//   -skeletal  keep skeletal models as they are (bones, weights, motions)
//   -static    strip the bones of skeletal props and build a plain static mesh,
//              the usual way to get physics props of the game into the static library
//   -mu        mark objects as Multiple Usage (instanced props with LODs)
// Without -skeletal or -static, skeletal models are skipped.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "xr_ogf.h"
#include "xr_utils.h"

namespace fs = std::filesystem;
using namespace xray_re;

namespace
{
	struct SOptions
	{
		bool keep_skeletal = false;
		bool make_static = false;
		bool multiple_usage = false;
	};

	struct SStats
	{
		unsigned converted = 0;
		unsigned skipped = 0;
		unsigned failed = 0;
	};

	bool ConvertOne(const fs::path& src, const fs::path& dst, const SOptions& options, SStats& stats)
	{
		xr_ogf* ogf = nullptr;
		try
		{
			ogf = xr_ogf::load_ogf(src.string());
		}
		catch (...)
		{
			ogf = nullptr;
		}

		if (!ogf)
		{
			std::printf("! can't load %s\n", src.string().c_str());
			++stats.failed;
			return false;
		}

		const bool skeletal = ogf->skeletal();
		if (skeletal && !options.keep_skeletal && !options.make_static)
		{
			std::printf("- skip skeletal %s\n", src.string().c_str());
			++stats.skipped;
			delete ogf;
			return true;
		}

		bool ok = false;
		try
		{
			if (skeletal && options.make_static)
			{
				// Weight maps are only built for objects with bones, so the plain
				// mesh path of xr_ogf yields static geometry once the bones are gone
				clear_container(ogf->bones());
				clear_container(ogf->motions());
				clear_container(ogf->partitions());
				ogf->xr_ogf::to_object();
				ogf->flags() = EOF_STATIC;
			}
			else
			{
				ogf->to_object();
			}

			if (options.multiple_usage && !(ogf->flags() & EOF_DYNAMIC))
				ogf->flags() = EOF_MULTIPLE_USAGE;

			std::error_code ec;
			fs::create_directories(dst.parent_path(), ec);
			ok = ogf->save_object(dst.string().c_str());
		}
		catch (...)
		{
			ok = false;
		}
		delete ogf;

		if (ok)
		{
			std::printf("+ %s\n", dst.string().c_str());
			++stats.converted;
		}
		else
		{
			std::printf("! can't convert %s\n", src.string().c_str());
			++stats.failed;
		}
		return ok;
	}
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::printf("usage: xrOGFConverter <model.ogf | folder> <output folder> [-skeletal] [-static] [-mu]\n");
		return 1;
	}

	const fs::path input = argv[1];
	const fs::path output = argv[2];
	SOptions options;
	for (int i = 3; i < argc; ++i)
	{
		if (0 == std::strcmp(argv[i], "-skeletal"))
			options.keep_skeletal = true;
		else if (0 == std::strcmp(argv[i], "-static"))
			options.make_static = true;
		else if (0 == std::strcmp(argv[i], "-mu"))
			options.multiple_usage = true;
	}

	SStats stats;
	std::error_code ec;
	if (fs::is_directory(input, ec))
	{
		for (const fs::directory_entry& entry : fs::recursive_directory_iterator(input, ec))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".ogf")
				continue;

			fs::path dst = output / fs::relative(entry.path(), input, ec);
			dst.replace_extension(".object");
			ConvertOne(entry.path(), dst, options, stats);
		}
	}
	else
	{
		fs::path dst = output / input.filename();
		dst.replace_extension(".object");
		ConvertOne(input, dst, options, stats);
	}

	std::printf("converted %u, skipped %u, failed %u\n", stats.converted, stats.skipped, stats.failed);
	return stats.failed ? 2 : 0;
}
