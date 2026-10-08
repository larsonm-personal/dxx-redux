extern "C" int run_cd_preview_completion(const char *trace_path);

int main(int argc, char **argv)
{
	return argc == 2 ? run_cd_preview_completion(argv[1]) : 2;
}
