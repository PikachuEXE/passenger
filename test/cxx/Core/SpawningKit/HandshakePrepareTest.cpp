#include <TestSupport.h>
#include <Core/SpawningKit/Handshake/Prepare.h>
#include <unistd.h>
#include <Utils.h>

using namespace Passenger;
using namespace Passenger::SpawningKit;

namespace tut {
	struct Core_SpawningKit_HandshakePrepareTest {
		SpawningKit::Context::Schema schema;
		SpawningKit::Context context;
		SpawningKit::Config config;
		boost::shared_ptr<HandshakeSession> session;

		Core_SpawningKit_HandshakePrepareTest()
			: context(schema)
		{
			context.resourceLocator = resourceLocator;
			context.integrationMode = "standalone";
			context.finalize();

			config.appRoot = "/tmp/myapp";
			config.startCommand = "echo hi";
			config.startupFile = "/tmp/myapp/app.py";
			config.appType = "wsgi";
			config.spawnMethod = "direct";
			config.user = getProcessUsername();
			config.group = getGroupName(getgid());
			config.internStrings();
		}

		void init(JourneyType type) {
			vector<StaticString> errors;
			ensure("Config is valid", config.validate(errors));
			session = boost::make_shared<HandshakeSession>(context, config, type);
			session->journey.setStepInProgress(SPAWNING_KIT_PREPARATION);
		}

		void initAndExec(JourneyType type, const Json::Value &extraArgs = Json::Value()) {
			init(type);
			HandshakePrepare(*session, extraArgs).execute();
		}
	};

	DEFINE_TEST_GROUP(Core_SpawningKit_HandshakePrepareTest);

	TEST_METHOD(1) {
		set_test_name("It resolves the user and group ID");

		initAndExec(SPAWN_DIRECTLY);

		ensure_equals("UID is resolved", session->uid, getuid());
		ensure_equals("GID is resolved", session->gid, getgid());
		ensure("Home dir is resolved", !session->homedir.empty());
		ensure("Shell is resolved", !session->shell.empty());
	}

	TEST_METHOD(2) {
		set_test_name("It raises an error if the user does not exist");

		config.user = "doesnotexist";
		try {
			initAndExec(SPAWN_DIRECTLY);
			fail("Exception expected");
		} catch (const SpawnException &) {
			// Pass
		}
	}

	TEST_METHOD(3) {
		set_test_name("It raises an error if the group does not exist");

		config.group = "doesnotexist";
		try {
			initAndExec(SPAWN_DIRECTLY);
			fail("Exception expected");
		} catch (const SpawnException &) {
			// Pass
		}
	}

	TEST_METHOD(4) {
		set_test_name("It raises an error if the user does not have a access to one "
			"of the app root's parent directories, or the app root itself");

		fail();

		#if 0
		runShellCommand("mkdir -p tmp.check/a/b/c");
		TempDirCopy dir("stub/rack", "tmp.check/a/b/c/d");
		TempDir dir2("tmp.check");

		char buffer[PATH_MAX];
		string cwd = getcwd(buffer, sizeof(buffer));

		SpawningKit::AppPoolOptions options = createOptions();
		options.appRoot = "tmp.check/a/b/c/d";
		options.appType = "rack";
		SpawnerPtr spawner = createSpawner(options);
		setLogLevel(LVL_CRIT);

		if (getuid() != 0) {
			// TODO: implement this test for root too
			runShellCommand("chmod 000 tmp.check/a/b/c/d");
			runShellCommand("chmod 600 tmp.check/a/b/c");
			runShellCommand("chmod 600 tmp.check/a");

			try {
				spawner->spawn(options);
				fail("SpawnException expected");
			} catch (const SpawnException &e) {
				ensure("(1)", containsSubstring(e.getErrorPage(),
					"the parent directory '" + cwd + "/tmp.check/a' has wrong permissions"));
			}

			runShellCommand("chmod 700 tmp.check/a");
			try {
				spawner->spawn(options);
				fail("SpawnException expected");
			} catch (const SpawnException &e) {
				ensure("(2)", containsSubstring(e.getErrorPage(),
					"the parent directory '" + cwd + "/tmp.check/a/b/c' has wrong permissions"));
			}

			runShellCommand("chmod 700 tmp.check/a/b/c");
			try {
				spawner->spawn(options);
				fail("SpawnException expected");
			} catch (const SpawnException &e) {
				ensure("(3)", containsSubstring(e.getErrorPage(),
					"However this directory is not accessible because it has wrong permissions."));
			}

			runShellCommand("chmod 700 tmp.check/a/b/c/d");
			spawner->spawn(options); // Should not throw.
		}
		#endif
	}

	TEST_METHOD(5) {
		set_test_name("It creates a work directory");

		initAndExec(SPAWN_DIRECTLY);

		ensure_equals(getFileType(session->workDir->getPath()), FT_DIRECTORY);
		ensure_equals(getFileType(session->workDir->getPath() + "/response"), FT_DIRECTORY);
	}

	TEST_METHOD(6) {
		set_test_name("It infers the application code revision from a REVISION file");

		TempDir tempdir("tmp.app");
		createFile("tmp.app/REVISION", "myversion\n");
		config.appRoot = "tmp.app";
		initAndExec(SPAWN_DIRECTLY);

		ensure_equals(session->result.codeRevision, "myversion");
	}

	TEST_METHOD(7) {
		set_test_name("It infers the application code revision from the"
			" Capistrano-style symlink in the app root path");

		TempDir tempdir("tmp.app");
		makeDirTree("tmp.app/myversion");
		symlink("myversion", "tmp.app/current");
		config.appRoot = "tmp.app/current";
		initAndExec(SPAWN_DIRECTLY);

		ensure_equals(session->result.codeRevision, "myversion");
	}

	TEST_METHOD(10) {
		set_test_name("In case of a generic app, it finds a free port for the app to listen on");

		unsigned long long timeout = 1000000;
		config.genericApp = true;
		initAndExec(SPAWN_DIRECTLY);

		Json::Value doc = context.inspectConfig();
		ensure("Port found", session->expectedStartPort > 0);
		ensure("Port is within range (1)",
			session->expectedStartPort >= doc["min_port_range"]["effective_value"].asUInt());
		ensure("Port is within range (2)",
			session->expectedStartPort <= doc["max_port_range"]["effective_value"].asUInt());
		ensure("Port is not used",
			!pingTcpServer("127.0.0.1", session->expectedStartPort, &timeout));
	}

	TEST_METHOD(11) {
		set_test_name("If findFreePort is true, it finds a free port for the app to listen on");

		unsigned long long timeout = 1000000;
		config.findFreePort = true;
		initAndExec(SPAWN_DIRECTLY);

		Json::Value doc = context.inspectConfig();
		ensure("Port found", session->expectedStartPort > 0);
		ensure("Port is within range (1)",
			session->expectedStartPort >= doc["min_port_range"]["effective_value"].asUInt());
		ensure("Port is within range (2)",
			session->expectedStartPort <= doc["max_port_range"]["effective_value"].asUInt());
		ensure("Port is not used",
			!pingTcpServer("127.0.0.1", session->expectedStartPort, &timeout));
	}

	TEST_METHOD(15) {
		set_test_name("It dumps arguments into the work directory");

		initAndExec(SPAWN_DIRECTLY);

		ensure(fileExists(session->workDir->getPath() + "/args.json"));
		ensure(fileExists(session->workDir->getPath() + "/args/app_root"));
		ensure_equals(readAll(session->workDir->getPath() + "/args/app_root"), config.appRoot);
	}

	struct Test16DebugSupport: public HandshakePrepare::DebugSupport {
		virtual void beforeAdjustTimeout() {
			usleep(100000);
		}
	};

	TEST_METHOD(16) {
		set_test_name("It adjusts the timeout when done");

		Test16DebugSupport debugSupport;
		config.startTimeoutMsec = 1000;
		init(SPAWN_DIRECTLY);
		HandshakePrepare preparation(*session);
		preparation.debugSupport = &debugSupport;
		preparation.execute();

		ensure("(1)", session->timeoutUsec <= 900000);
		ensure("(2)", session->timeoutUsec >= 100000);
	}

	struct Test17DebugSupport: public HandshakePrepare::DebugSupport {
		virtual void beforeAdjustTimeout() {
			throw RuntimeException("oh no");
		}
	};

	TEST_METHOD(17) {
		set_test_name("Upon throwing an exception, it sets the SPAWNING_KIT_PREPARATION step to errored");

		Test17DebugSupport debugSupport;
		init(SPAWN_DIRECTLY);
		HandshakePrepare preparation(*session);
		preparation.debugSupport = &debugSupport;

		try {
			preparation.execute();
			fail("SpawnException expected");
		} catch (const SpawnException &) {
			ensure_equals(session->journey.getFirstFailedStep(), SPAWNING_KIT_PREPARATION);
		}
	}
}
