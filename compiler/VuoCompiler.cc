﻿/**
 * @file
 * VuoCompiler implementation.
 *
 * @copyright Copyright © 2012–2013 Kosada Incorporated.
 * This code may be modified and distributed under the terms of the GNU Lesser General Public License (LGPL) version 2 or later.
 * For more information, see http://vuo.org/license.
 */

#include <fcntl.h>
#include <mach-o/dyld.h>
#include <limits.h>			/* PATH_MAX */
#include <string.h>
#include "VuoCompiler.hh"
#include "VuoCompilerBitcodeGenerator.hh"
#include "VuoCompilerCodeGenUtilities.hh"
#include "VuoCompilerMakeListNodeClass.hh"
#include "VuoCompilerNodeClass.hh"
#include "VuoFileUtilities.hh"
#include "VuoStringUtilities.hh"


VuoCompiler::VuoCompiler()
{
	llvm::InitializeNativeTarget();

	// If the Vuo compiler/linker...
	//   1. Loads a node class that uses dispatch_object_t.
	//   2. Generates code that uses dispatch_object_t.
	//   3. Links the node class into a composition.
	//   4. Generates more code that uses dispatch_object_t.
	// ... then Step 4 ends up with the wrong llvm::Type for dispatch_object_t.
	//
	// A workaround is to generate some code that uses dispatch_object_t before doing Step 1.
	//
	// https://b33p.net/kosada/node/3845
	// http://lists.cs.uiuc.edu/pipermail/llvmdev/2012-December/057075.html
	Module module("", getGlobalContext());
	VuoCompilerCodeGenUtilities::getDispatchObjectType(&module);

	string vuoFrameworkPath = getVuoFrameworkPath().str();
	if (! vuoFrameworkPath.empty())
	{
		addPreferredLibrarySearchPath(vuoFrameworkPath + "/Frameworks/graphviz.framework/");
		addPreferredLibrarySearchPath(vuoFrameworkPath + "/Frameworks/json.framework/");
		addPreferredLibrarySearchPath(vuoFrameworkPath + "/Frameworks/zmq.framework/");
		addPreferredLibrarySearchPath(vuoFrameworkPath + "/Frameworks/CRuntime.framework/");
		addPreferredLibrarySearchPath(vuoFrameworkPath + "/Frameworks/VuoRuntime.framework/");
		addPreferredLibrarySearchPath(vuoFrameworkPath + "/Modules/");

		addModuleSearchPath(vuoFrameworkPath + "/Modules/");

		addPreferredFrameworkSearchPath(vuoFrameworkPath + "/Frameworks/");

		clangPath = vuoFrameworkPath + "/MacOS/Clang/bin/clang";
	}
	else
	{
		addModuleSearchPath(VUO_ROOT "/node");
		addModuleSearchPath(VUO_ROOT "/type");
		addModuleSearchPath(VUO_ROOT "/type/list");

		// /usr/local/lib is already in the search path.
		// Since we don't have Vuo.framework, tell ld where it can find libgvplugin_core.dylib and libgvplugin_dot_layout.dylib.
		addLibrarySearchPath(GRAPHVIZ_ROOT "/lib/graphviz/");
		addLibrarySearchPath(ICU_ROOT "/lib/");
		addLibrarySearchPath(JSONC_ROOT "/lib/");
		addLibrarySearchPath(ZMQ_ROOT "/lib/");
		addLibrarySearchPath(LEAP_ROOT);
		addLibrarySearchPath(MUPARSER_ROOT "/lib/");
		addLibrarySearchPath(FREEIMAGE_ROOT "/lib/");
		addLibrarySearchPath(FREETYPE_ROOT "/lib/");
		addLibrarySearchPath(CURL_ROOT "/lib/");
		addLibrarySearchPath(RTMIDI_ROOT "/lib/");
		addLibrarySearchPath(ASSIMP_ROOT "/lib/");
		addLibrarySearchPath(VUO_ROOT "/runtime");

		clangPath = llvm::sys::Path(StringRef(LLVM_ROOT "/bin/clang"));
	}

	isVerbose = false;

	// Allow system administrator to override Vuo.framework modules
	addModuleSearchPath(getSystemModulesPath());

	// Allow user to override Vuo.framework and system-wide modules
	addModuleSearchPath(getUserModulesPath());
}

/**
 * Loads node classes and types from any directories in @c nodeClassSearchPaths that have not already been loaded.
 *
 * In this class, the modules for node classes and types (@c nodeClasses and @c types) are loaded lazily.
 * They are loaded the first time they are accessed (by strategically placed calls to this function).
 * Specifically, they are loaded the first time compiling a composition, linking a composition, or otherwise
 * accessing the collections of node classes or types. They are not loaded when compiling a node class.
 */
void VuoCompiler::loadModulesIfNeeded(void)
{
	for (vector<string>::iterator i = moduleSearchPaths.begin(); i != moduleSearchPaths.end(); ++i)
	{
		string path = *i;
		if (! isNodeClassAndTypeSearchPathLoaded[path])
		{
			loadModules(path);
			isNodeClassAndTypeSearchPathLoaded[path] = true;
		}
	}
}

/**
 * Loads all node classes, types, and library modules in the directory at @c path, which is recursively searched.
 * Adds @c path to the list of library search paths used in linking.
 *
 * If multiple definitions of a node class or type are encountered (either in different folders
 * inside of @c path or in different calls to this function), the most recently encountered
 * definition is used.
 *
 * If @c path does not exist, does nothing.
 */
void VuoCompiler::loadModules(string path)
{
	set<string> fileRelativePaths = VuoFileUtilities::findFilesInDirectory(path, "bc");
	set<string> nodes = VuoFileUtilities::findFilesInDirectory(path, "vuonode");
	fileRelativePaths.insert(nodes.begin(), nodes.end());
	for (set<string>::iterator i = fileRelativePaths.begin(); i != fileRelativePaths.end(); ++i)
	{
		string fileFullPath = path + "/" + *i;

		string moduleKey = getModuleNameForPath(fileFullPath);
		Module *module = readModuleFromBitcode(fileFullPath);
		VuoCompilerModule *compilerModule = VuoCompilerModule::newModule(moduleKey, module);

		if (compilerModule)
		{
			if (dynamic_cast<VuoCompilerNodeClass *>(compilerModule))
				nodeClasses[moduleKey] = static_cast<VuoCompilerNodeClass *>(compilerModule);
			else if (dynamic_cast<VuoCompilerType *>(compilerModule))
				types[moduleKey] = static_cast<VuoCompilerType *>(compilerModule);
			else
				libraryModules[moduleKey] = compilerModule;
		}
	}

	reifyPortTypes();
}

/**
 * Adds a node class to use when linking a composition.
 *
 * Typically, node classes are loaded from file with addModuleSearchPath().
 * This function is useful if adding a node class that is generated at compile time.
 */
void VuoCompiler::addNodeClass(VuoCompilerNodeClass *nodeClass)
{
	setTargetForModule(nodeClass->getModule());

	nodeClasses[nodeClass->getBase()->getModuleKey()] = nodeClass;

	reifyPortTypes();
}

/**
 * Updates the data-and-event ports of each known node class to match them up with known types.
 * This method needs to be called between when the last node class or type is loaded and
 * when a composition is compiled. It can be called multiple times.
 */
void VuoCompiler::reifyPortTypes(void)
{
	for (map<string, VuoCompilerNodeClass *>::iterator i = nodeClasses.begin(); i != nodeClasses.end(); ++i)
	{
		VuoNodeClass *nodeClass = i->second->getBase();

		vector<VuoPortClass *> inputPortClasses = nodeClass->getInputPortClasses();
		vector<VuoPortClass *> outputPortClasses = nodeClass->getOutputPortClasses();
		vector<VuoPortClass *> portClasses;
		portClasses.insert(portClasses.end(), inputPortClasses.begin(), inputPortClasses.end());
		portClasses.insert(portClasses.end(), outputPortClasses.begin(), outputPortClasses.end());

		for (vector<VuoPortClass *>::iterator j = portClasses.begin(); j != portClasses.end(); ++j)
		{
			VuoCompilerPortClass *portClass = static_cast<VuoCompilerPortClass *>((*j)->getCompiler());
			VuoType *baseType = portClass->getDataVuoType();

			if (baseType && ! baseType->hasCompiler())
			{
				map<string, VuoCompilerType *>::iterator compilerTypeIter = types.find(baseType->getModuleKey());
				if (compilerTypeIter != types.end())
				{
					VuoCompilerType *compilerType = compilerTypeIter->second;
					portClass->setDataVuoType(compilerType->getBase());
				}
			}
		}
	}
}

/**
 * Compiles a node class, port type, or library module to LLVM bitcode.
 *
 * @param inputPath The file to compile, containing a C implementation of the node class, port type, or library module.
 * @param outputPath The file in which to save the compiled LLVM bitcode.
 */
void VuoCompiler::compileModule(string inputPath, string outputPath)
{
	if (isVerbose)
		print();

	Module *module = readModuleFromC(inputPath);
	if (! module)
	{
		fprintf(stderr, "Couldn't compile %s to LLVM bitcode.\n", inputPath.c_str());
		return;
	}

	string moduleKey = getModuleNameForPath(inputPath);
	VuoCompilerModule *compilerModule = VuoCompilerModule::newModule(moduleKey, module);
	if (! compilerModule)
	{
		fprintf(stderr, "Didn't recognize %s as a node class, type, or library.\n", inputPath.c_str());
		return;
	}

	setTargetForModule(module, target);
	writeModuleToBitcode(module, outputPath);
}

/**
 * Compiles a composition to LLVM bitcode.
 *
 * @param generator A bitcode generator for the composition.
 * @param outputPath The file in which to save the compiled LLVM bitcode.
 */
void VuoCompiler::compileComposition(VuoCompilerBitcodeGenerator *generator, string outputPath)
{
	if (telemetry == "console")
		generator->setDebugMode(true);

	Module *module = generator->generateBitcode();
	setTargetForModule(module, target);
	writeModuleToBitcode(module, outputPath);
}

/**
 * Compiles a composition, read from file, to LLVM bitcode.
 *
 * @param inputPath The .vuo file containing the composition.
 * @param outputPath The file in which to save the compiled LLVM bitcode.
 */
void VuoCompiler::compileComposition(string inputPath, string outputPath)
{
	if (isVerbose)
		print();

	VuoCompilerBitcodeGenerator *generator = VuoCompilerBitcodeGenerator::newBitcodeGeneratorFromCompositionFile(inputPath, this);
	compileComposition(generator, outputPath);
	delete generator;
}

/**
 * Compiles the composition, read from a string, to LLVM bitcode.
 *
 * @param compositionString A string containing the composition.
 * @param outputPath The file in which to save the compiled LLVM bitcode.
 */
void VuoCompiler::compileCompositionString(const string &compositionString, string outputPath)
{
	VuoCompilerBitcodeGenerator *generator = VuoCompilerBitcodeGenerator::newBitcodeGeneratorFromCompositionString(compositionString, this);
	compileComposition(generator, outputPath);
	delete generator;
}

/**
 * Turns a compiled composition into a standalone executable by
 * linking in all of its dependencies and adding a main function.
 *
 * @param inputPath Path to the compiled composition (an LLVM bitcode file).
 * @param outputPath Path where the resulting executable should be placed.
 */
void VuoCompiler::linkCompositionToCreateExecutable(string inputPath, string outputPath)
{
	linkCompositionToCreateExecutableOrDynamicLibrary(inputPath, outputPath, false);
}

/**
 * Turns a compiled composition into a dynamic library by
 * linking in all of its dependencies.
 *
 * @param inputPath Path to the compiled composition (an LLVM bitcode file).
 * @param outputPath Path where the resulting dynamic library should be placed.
 */
void VuoCompiler::linkCompositionToCreateDynamicLibrary(string inputPath, string outputPath)
{
	linkCompositionToCreateExecutableOrDynamicLibrary(inputPath, outputPath, true);
}

/**
 * Creates an executable or dynamic library that contains the composition and its dependencies.
 *
 * If creating an executable, a main function is added.
 *
 * @param compiledCompositionPath Path to the compiled composition (n LLVM bitcode file).
 * @param linkedCompositionPath Path where the resulting executable or dynamic library should be placed.
 * @param isDylib True if creating a dynamic library, false if creating an executable.
 */
void VuoCompiler::linkCompositionToCreateExecutableOrDynamicLibrary(string compiledCompositionPath, string linkedCompositionPath, bool isDylib)
{
	if (isVerbose)
		print();

	set<string> dependencies = getDependenciesForComposition(compiledCompositionPath);
	dependencies.insert(getRuntimeDependency());
	if (! isDylib)
		dependencies.insert(getRuntimeMainDependency());
	set<Module *> modules;
	set<string> libraries;
	set<string> frameworks;
	getLinkerInputs(dependencies, modules, libraries, frameworks);

	libraries.insert(compiledCompositionPath);

	link(linkedCompositionPath, modules, libraries, frameworks, isDylib);
}

/**
 * Creates one dynamic library for the composition by itself and, if needed, another dynamic library for the
 * node classes and other resources that are dependencies of the composition.
 *
 * @param compiledCompositionPath Path to the compiled composition (an LLVM bitcode file).
 * @param linkedCompositionPath Path where the resulting dynamic library for the composition should be placed.
 * @param newLinkedResourcePath Path where the resulting dynamic library for the composition's resources should be placed.
 *				The dynamic library is only created if this version of the composition requires resources that are not in
 *				@c alreadyLinkedResources. When this function returns, if the dynamic library was created, then this
 *				argument is the same as when it was passed in; otherwise, this argument is the empty string.
 * @param alreadyLinkedResourcePaths Paths where the resulting dynamic libraries for the composition's resources have
 *				been placed in previous calls to this function. When this function returns, if a dynamic library was
 *				created at @c newLinkedResourcePath, it will be the last element in this list.
 * @param alreadyLinkedResources Names of resources that have been linked into the composition in previous calls to this
 *				function. When this function returns, any new resources will have been added to this list.
 */
void VuoCompiler::linkCompositionToCreateDynamicLibraries(string compiledCompositionPath, string linkedCompositionPath,
														  string &newLinkedResourcePath, vector<string> &alreadyLinkedResourcePaths,
														  set<string> &alreadyLinkedResources)
{
	if (isVerbose)
		print();

	// Get the dependencies used by the new resources and not the previous resources.
	set<string> newDependencies = getDependenciesForComposition(compiledCompositionPath);
	set<string> newDependenciesCopy = newDependencies;
	for (set<string>::iterator i = newDependenciesCopy.begin(); i != newDependenciesCopy.end(); ++i)
		if (alreadyLinkedResources.find(*i) != alreadyLinkedResources.end())
			newDependencies.erase(newDependencies.find(*i));

	// Get the dynamic libraries and frameworks used by the new and previous resources.
	set<string> dylibs;
	set<string> frameworks;
	{
		set<Module *> modules;
		set<string> libraries;

		alreadyLinkedResources.insert(newDependencies.begin(), newDependencies.end());

		getLinkerInputs(alreadyLinkedResources, modules, libraries, frameworks);

		for (set<string>::iterator i = libraries.begin(); i != libraries.end(); ++i)
			if (VuoStringUtilities::endsWith(*i, ".dylib"))
				dylibs.insert(*i);
	}

	// Link the new resource dylib, if needed.
	if (! newDependencies.empty())
	{
		set<Module *> modules;
		set<string> libraries;

		libraries.insert(alreadyLinkedResourcePaths.begin(), alreadyLinkedResourcePaths.end());
		libraries.insert(dylibs.begin(), dylibs.end());
		getLinkerInputs(newDependencies, modules, libraries, frameworks);

		link(newLinkedResourcePath, modules, libraries, frameworks, true);

		alreadyLinkedResourcePaths.push_back(newLinkedResourcePath);
	}
	else
	{
		newLinkedResourcePath = "";
	}

	// Get the Vuo runtime dependency.
	string vuoRuntimePath;
	{
		set<Module *> modules;
		set<string> libraries;
		set<string> frameworks;

		set<string> dependencies;
		dependencies.insert(getRuntimeDependency());
		getLinkerInputs(dependencies, modules, libraries, frameworks);
		vuoRuntimePath = *libraries.begin();
	}

	// Link the composition.
	{
		set<Module *> modules;
		set<string> libraries;

		libraries.insert(compiledCompositionPath);
		libraries.insert(vuoRuntimePath);
		libraries.insert(alreadyLinkedResourcePaths.begin(), alreadyLinkedResourcePaths.end());
		libraries.insert(dylibs.begin(), dylibs.end());
		link(linkedCompositionPath, modules, libraries, frameworks, true);
	}
}

/**
 * Returns the names of dependencies (node classes, types, libraries, and frameworks)
 * needed for linking the composition.
 *
 * This includes the composition's nodes, their dependencies, and the libraries needed
 * by every linked composition. It does not include the Vuo runtime or a main function.
 */
set<string> VuoCompiler::getDependenciesForComposition(const string &compiledCompositionPath)
{
	loadModulesIfNeeded();

	set<string> dependencies;

	// Add the node classes in the top-level composition and their dependencies.
	Module *compositionModule = readModuleFromBitcode(compiledCompositionPath);
	VuoCompilerBitcodeParser parser(compositionModule);
	vector<string> directDependencies = parser.getStringsFromGlobalArray("moduleDependencies");
	for (vector<string>::iterator i = directDependencies.begin(); i != directDependencies.end(); ++i)
	{
		string dependency = *i;
		set<string> dependenciesToAdd;
		getDependenciesRecursively(dependency, dependenciesToAdd);
		dependencies.insert(dependenciesToAdd.begin(), dependenciesToAdd.end());
	}

	// Add the libraries needed by every linked composition.
	vector<string> coreDependencies = getCoreVuoDependencies();
	dependencies.insert(coreDependencies.begin(), coreDependencies.end());

	return dependencies;
}

/**
 * Adds @c dependency and the names of all of its dependencies to @c dependencies.
 *
 * @param dependency The current dependency being searched for its dependencies.
 * @param[out] dependencies The dependencies found so far by calls to this function.
 * @throw std::runtime_error At least one of the dependencies is incompatible with the targets for building the composition.
 */
void VuoCompiler::getDependenciesRecursively(const string &dependency, set<string> &dependencies)
{
	if (dependencies.find(dependency) != dependencies.end())
		return;

	// If the composition was compiled with a different VuoCompiler instance, then node classes
	// that were generated at compile time need to be re-generated for this VuoCompiler instance.
	if (VuoCompilerMakeListNodeClass::isMakeListNodeClassName(dependency))
		VuoCompilerMakeListNodeClass::getNodeClass(dependency, this);

	// Add the dependency itself.
	dependencies.insert(dependency);

	// Determine if the dependency is a node class, type, library module, or none of the above.
	VuoCompilerModule *module = NULL;
	map<string, VuoCompilerNodeClass *>::iterator nodeClassIter = nodeClasses.find(dependency);
	if (nodeClassIter != nodeClasses.end())
		module = nodeClassIter->second;
	else
	{
		map<string, VuoCompilerType *>::iterator typeIter = types.find(dependency);
		if (typeIter != types.end())
			module = typeIter->second;
		else
		{
			map<string, VuoCompilerModule *>::iterator libraryModuleIter = libraryModules.find(dependency);
			if (libraryModuleIter != libraryModules.end())
				module = libraryModuleIter->second;
		}
	}

	if (module)
	{
		// Check that the module is compatible with the targets.
		VuoCompilerTargetSet compositionTargets;
		compositionTargets.restrictToCurrentOperatingSystemVersion();
		if (! module->getCompatibleTargets().isCompatibleWithAllOf(compositionTargets))
			throw std::runtime_error(dependency + " is not compatible with all of the operating systems that this composition needs to run on. \n" +
									 dependency + " is compatible with: " + module->getCompatibleTargets().toString() + "\n" +
									 "This composition is compatible with: " + compositionTargets.toString());

		// Add the module's dependencies.
		vector<string> directDependencies = module->getDependencies();
		for (vector<string>::iterator i = directDependencies.begin(); i != directDependencies.end(); ++i)
		{
			string directDependency = *i;
			getDependenciesRecursively(directDependency, dependencies);
		}

		/// @todo Only add the types needed by this module. (https://b33p.net/kosada/node/3021)
		for (map<string, VuoCompilerType *>::iterator i = types.begin(); i != types.end(); ++i)
		{
			string typeDependency = i->first;
			getDependenciesRecursively(typeDependency, dependencies);
		}
	}
}

/**
 * From a list of names of dependencies, gets the modules, library paths, and frameworks
 * to be passed to the linker.
 */
void VuoCompiler::getLinkerInputs(const set<string> &dependencies,
								  set<Module *> &modules, set<string> &libraries, set<string> &frameworks)
{
	// Use separate linkers for custom versus system search paths to control the order in which paths are searched.
	// Linker::addPaths() adds paths to the end, but Linker::addSystemPaths() adds them to the beginning.
	Linker linker("", "", getGlobalContext());
	linker.addPaths(preferredLibrarySearchPaths);
	linker.addPaths(librarySearchPaths);
	Linker systemLinker("", "", getGlobalContext());
	systemLinker.addSystemPaths();

	for (set<string>::iterator i = dependencies.begin(); i != dependencies.end(); ++i)
	{
		string dependency = *i;

		map<string, VuoCompilerNodeClass *>::iterator nodeClassIter = nodeClasses.find(dependency);
		if (nodeClassIter != nodeClasses.end())
			modules.insert(nodeClassIter->second->getModule());
		else
		{
			map<string, VuoCompilerType *>::iterator typeIter = types.find(dependency);
			if (typeIter != types.end())
				modules.insert(typeIter->second->getModule());
			else
			{
				map<string, VuoCompilerModule *>::iterator libraryModuleIter = libraryModules.find(dependency);
				if (libraryModuleIter != libraryModules.end())
					modules.insert(libraryModuleIter->second->getModule());
				else
				{
					if (VuoStringUtilities::endsWith(dependency, ".framework"))
						frameworks.insert(dependency);
					else
					{
						sys::Path dependencyPath = linker.FindLib(dependency);
						if (dependencyPath.isEmpty())
							dependencyPath = systemLinker.FindLib(dependency);
						if (! dependencyPath.isEmpty())
							libraries.insert(dependencyPath.str());
						else
							fprintf(stderr, "Warning: Could not locate dependency \'%s\'\n", dependency.c_str());
					}
				}
			}
		}
	}
}

/**
 * Links the given modules, libraries, and frameworks to create an executable or dynamic library.
 *
 * @param outputPath The resulting executable or dynamic library.
 * @param modules The LLVM modules to link in.
 * @param libraries The libraries to link in. If building an executable, one of them should contain a main function.
 * @param frameworks The frameworks to link in.
 * @param isDylib If true, the output file will be a dynamic library. Otherwise, it will be an executable.
 */
void VuoCompiler::link(string outputPath, const set<Module *> &modules, const set<string> &libraries, const set<string> &frameworks, bool isDylib)
{
	// http://stackoverflow.com/questions/11657529/how-to-generate-an-executable-from-an-llvmmodule


	// Write all the modules with renamed symbols to a composite module file (since the linker can't operate on in-memory modules).
	Module *compositeModule = new Module("composite", getGlobalContext());
	setTargetForModule(compositeModule);
	for (set<Module *>::const_iterator i = modules.begin(); i != modules.end(); ++i)
	{
		string error;
		if (Linker::LinkModules(compositeModule, *i, Linker::PreserveSource, &error))
			fprintf(stderr, "VuoCompiler::link() compositeModule error: %s\n", error.c_str());
	}
	string compositeModulePath = VuoFileUtilities::makeTmpFile("composite", "bc");
	writeModuleToBitcode(compositeModule, compositeModulePath);


	// llvm-3.1/llvm/tools/clang/tools/driver/driver.cpp

	llvm::sys::Path clangPath = getClangPath();

	vector<const char *> args;
	args.push_back(clangPath.c_str());

	args.push_back(compositeModulePath.c_str());

	for (set<string>::const_iterator i = libraries.begin(); i != libraries.end(); ++i)
		args.push_back(i->c_str());

	// Add framework search paths
	vector<string> frameworkArguments;

	for (vector<string>::const_iterator i = preferredFrameworkSearchPaths.begin(); i != preferredFrameworkSearchPaths.end(); ++i)
	{
		string a = "-F"+*i;
		// Keep these std::strings around until after args is done being used, since the std::string::c_str() is freed when the std::string is deleted.
		frameworkArguments.push_back(a);
		args.push_back(a.c_str());
	}

	for (vector<string>::const_iterator i = frameworkSearchPaths.begin(); i != frameworkSearchPaths.end(); ++i)
	{
		string a = "-F"+*i;
		// Keep these std::strings around until after args is done being used, since the std::string::c_str() is freed when the std::string is deleted.
		frameworkArguments.push_back(a);
		args.push_back(a.c_str());
	}

	for (set<string>::const_iterator i = frameworks.begin(); i != frameworks.end(); ++i)
	{
		args.push_back("-framework");

		string frameworkName = *i;
		frameworkName = frameworkName.substr(0, frameworkName.length() - string(".framework").length());
		args.push_back(strdup(frameworkName.c_str()));
	}

	// Check for C Runtime path within Vuo.framework
	llvm::sys::Path cRuntimePath;
	llvm::sys::Path crt1Path;
	string vuoFrameworkPath = getVuoFrameworkPath().str();
	string vuoFrameworkContainingFolder = vuoFrameworkPath + "/..";
	if (! vuoFrameworkPath.empty())
	{
		cRuntimePath = vuoFrameworkPath + "/Frameworks/CRuntime.framework/";
		crt1Path = cRuntimePath;
		crt1Path.appendComponent("crt1.o");
	}

	// If we have located a bundled version of crt1.o, link it in explicitly rather than relying on
	// clang's heuristic to locate a system version.
	if (!isDylib && crt1Path.canRead())
	{
		args.push_back("-nostartfiles");
		args.push_back(crt1Path.c_str());
	}

	// Linker option necessary for compatibility with our bundled version of ld64:
	args.push_back("-Xlinker");
	args.push_back("--no-demangle");

	// Avoid generating unknown load commands.
	// http://www.cocoabuilder.com/archive/xcode/308488-load-commands-in-10-7-dylib-block-install-name-tool-on-10-6.html
	args.push_back("-Xlinker");
	args.push_back("-no_function_starts");
	args.push_back("-Xlinker");
	args.push_back("-no_version_load_command");

	if (isVerbose)
		args.push_back("-v");

	if (isDylib)
		args.push_back("-dynamiclib");

	// Tell the built dylib/executable where to find Vuo.framework
	/// @todo Once we can build app bundles (https://b33p.net/kosada/node/3362), copy only the needed dynamic dependencies into the app bundle, and change the rpath accordingly
	args.push_back("-rpath");
	args.push_back(vuoFrameworkContainingFolder.c_str());

	// Allow clang to print meaningful error messages.
	clang::DiagnosticOptions *diagOptions = new clang::DiagnosticOptions();
	clang::TextDiagnosticPrinter *diagClient = new clang::TextDiagnosticPrinter(llvm::errs(), diagOptions);
	diagClient->setPrefix(clangPath.str());
	IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new clang::DiagnosticIDs());
	clang::DiagnosticsEngine Diags(DiagID, diagOptions, diagClient);

	clang::driver::Driver TheDriver(args[0], "x86_64-apple-macosx10.6.0", outputPath, true, Diags);

	TheDriver.CCCIsCXX = true;  // clang++ instead of clang

	OwningPtr<clang::driver::Compilation> C(TheDriver.BuildCompilation(args));

	int Res = 0;
	const clang::driver::Command *FailingCommand = 0;
	if (C)
		Res = TheDriver.ExecuteCompilation(*C, FailingCommand);

	if (Res < 0)
		TheDriver.generateCompilationDiagnostics(*C, FailingCommand);


	// Clean up composite module file.
	remove(compositeModulePath.c_str());
}

/**
 * Returns the LLVM module read from the node class, type, or library implementation at @c inputPath (a .c file).
 */
Module * VuoCompiler::readModuleFromC(string inputPath)
{
	// llvm-3.1/llvm/tools/clang/examples/clang-interpreter/main.cpp

	vector<const char *> args;
	args.push_back(inputPath.c_str());
	args.push_back("-DVUO_COMPILER");
	args.push_back("-fblocks");
	for (vector<string>::iterator i = headerSearchPaths.begin(); i != headerSearchPaths.end(); ++i)
	{
		args.push_back("-I");
		args.push_back(i->c_str());
	}
	if (isVerbose)
		args.push_back("-v");

	clang::DiagnosticOptions * diagOptions = new clang::DiagnosticOptions();
	IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new clang::DiagnosticIDs());
	clang::DiagnosticsEngine Diags(DiagID, diagOptions);

	OwningPtr<clang::CompilerInvocation> CI(new clang::CompilerInvocation);
	clang::CompilerInvocation::CreateFromArgs(*CI, &args[0], &args[0] + args.size(), Diags);

	clang::CompilerInstance Clang;
	Clang.setInvocation(CI.take());

	Clang.createDiagnostics(args.size(), &args[0]);
	if (!Clang.hasDiagnostics())
		return NULL;

	// See CompilerInvocation::GetResourcesPath -- though we're not calling it because we don't have MainAddr.
	llvm::sys::Path clangPath = getClangPath();
	llvm::sys::Path builtinHeaderSearchPath = clangPath;
	builtinHeaderSearchPath.eraseComponent();  // Remove /clang from foo/bin/clang
	builtinHeaderSearchPath.eraseComponent();  // Remove /bin   from foo/bin
	builtinHeaderSearchPath.appendComponent("lib");
	builtinHeaderSearchPath.appendComponent("clang");
	builtinHeaderSearchPath.appendComponent(CLANG_VERSION_STRING);  // foo/lib/clang/<version>
	Clang.getHeaderSearchOpts().ResourceDir = builtinHeaderSearchPath.str();

//	OwningPtr<clang::CodeGenAction> Act(new clang::EmitLLVMOnlyAction());  // @@@ return value of takeModule() is destroyed at the end of this function
	clang::CodeGenAction *Act = new clang::EmitLLVMOnlyAction();
	if (!Clang.ExecuteAction(*Act))
		return NULL;

	return Act->takeModule();
}

/**
 * Returns the LLVM module read from @c inputPath (a .bc file).
 */
Module * VuoCompiler::readModuleFromBitcode(string inputPath)
{
	OwningPtr<MemoryBuffer> mb;
	error_code err = MemoryBuffer::getFile(inputPath.c_str(), mb);
	if (err) {
		printf("Can't open '%s'.\n", inputPath.c_str());
		return NULL;
	}

	string error;
	Module *module = ParseBitcodeFile(&(*mb), getGlobalContext(), &error);
	if (! module) {
		fprintf(stderr, "Can't parse '%s': %s.\n", inputPath.c_str(), error.c_str());
		return NULL;
	}

	return module;
}

/**
 * Verifies the LLVM module and writes it to @c outputPath (a .bc file).
 *
 * Returns true if there was a problem verifying or writing the module.
 */
bool VuoCompiler::writeModuleToBitcode(Module *module, string outputPath)
{
	if (verifyModule(*module, PrintMessageAction))
	{
		fprintf(stderr, "Module verification failed.\n");
		return true;
	}

	string err;
	raw_fd_ostream out(outputPath.c_str(), err);
	if (! err.empty())
	{
		fprintf(stderr, "Couldn't open file %s (%s)\n", outputPath.c_str(), err.c_str());
		return true;
	}
	WriteBitcodeToFile(module, out);

	return false;
}

/**
 * Sets the target triple for @c module. If @c target is empty, uses the target triple of the host machine.
 */
void VuoCompiler::setTargetForModule(Module *module, string target)
{
/*
	string effectiveTarget = target;
	if (effectiveTarget.empty())
	{
		// llvm::sys::getDefaultTargetTriple() finds a target based on the host, but the "default" target is not necessarily the
		// same target that results from invoking command-line clang without a -target argument. That is the "effective" target.
		// For example, the "default" target could be x86_64-apple-darwin10.8.0 and the "effective" target could be x86_64-apple-macosx10.6.0.

		llvm::sys::Path clangPath = getClangPath();

		vector<const char *> args;
		args.push_back(clangPath.c_str());
		args.push_back("/bin/sh");  // Driver needs an input file (that exists) or it refuses to give you the correct effective target.

		clang::DiagnosticOptions * diagOptions = new clang::DiagnosticOptions();
		IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new clang::DiagnosticIDs());
		clang::DiagnosticsEngine Diags(DiagID, diagOptions);

		clang::driver::Driver TheDriver(args[0], llvm::sys::getDefaultTargetTriple(), "a.out", true, Diags);
		OwningPtr<clang::driver::Compilation> C(TheDriver.BuildCompilation(args));
		effectiveTarget = C->getDefaultToolChain().ComputeEffectiveClangTriple(C->getArgs());
	}

	module->setTargetTriple(effectiveTarget);
*/
	module->setTargetTriple("x86_64-apple-macosx10.6.0");
}

/**
 * Looks up the VuoCompilerNodeClass for the node class specified by @a id.
 *
 * The node class module is loaded if it hasn't been already.
 *
 * @eg{
 *		VuoCompiler *compiler = new VuoCompiler();
 *		VuoCompilerNodeClass *nc = compiler->getNodeClass("vuo.math.add.integer");
 *		[...]
 *		delete compiler;
 * }
 */
VuoCompilerNodeClass * VuoCompiler::getNodeClass(const string &id)
{
	loadModulesIfNeeded();

	map<string, VuoCompilerNodeClass *>::const_iterator i = nodeClasses.find(id);
	return (i != nodeClasses.end() ? i->second : NULL);
}

/**
 * Returns a map linking a string representation of a node class's name to its VuoCompilerNodeClass instance.
 *
 * The node class modules are loaded if they haven't been already.
 */
map<string, VuoCompilerNodeClass *> VuoCompiler::getNodeClasses()
{
	loadModulesIfNeeded();
	return nodeClasses;
}

/**
 * Looks up the VuoCompilerType for the port type specified by @a id.
 *
 * The port type module is loaded if it haven't been already.
 */
VuoCompilerType * VuoCompiler::getType(const string &id)
{
	loadModulesIfNeeded();

	map<string, VuoCompilerType *>::const_iterator i = types.find(id);
	return (i != types.end() ? i->second : NULL);
}

/**
 * Prints a list of all loaded node classes to standard output.
 *
 * The node class modules are loaded if they haven't been already.
 *
 * @param format The format for printing the node classes.
 *	- If "", prints each class name (e.g. vuo.math.count.integer), one per line.
 *	- If "path", prints the absolute path of each node class, one per line.
 *	- If "dot", prints the declaration of a node as it would appear in a .vuo (DOT format) file,
 *		with a constant value set for each data+event input port
 *		and a comment listing metadata and port types for the node class.
 */
void VuoCompiler::listNodeClasses(const string &format)
{
	loadModulesIfNeeded();

	for (map<string, VuoCompilerNodeClass *>::const_iterator i = nodeClasses.begin(); i != nodeClasses.end(); ++i)
	{
		VuoCompilerNodeClass *nodeClass = i->second;
		if (format == "")
		{
			printf("%s\n", nodeClass->getBase()->getClassName().c_str());
		}
		else if (format == "path")
		{
			// TODO
		}
		else if (format == "dot")
		{
			VuoCompilerNode *node = nodeClass->newNode()->getCompiler();

			printf("%s\n", nodeClass->getDoxygenDocumentation().c_str());
			printf("%s\n\n", node->getGraphvizDeclaration().c_str());

			delete node;
		}
	}
}

/**
 * Returns the file names of bitcode dependencies needed by every linked Vuo composition.
 */
vector<string> VuoCompiler::getCoreVuoDependencies(void)
{
	vector<string> dependencies;
	dependencies.push_back("VuoHeap.bc");
	dependencies.push_back("VuoTelemetry.bc");
	dependencies.push_back("zmq");
	dependencies.push_back("gvc");
	dependencies.push_back("graph");
	dependencies.push_back("cdt");
	dependencies.push_back("pathplan");
	dependencies.push_back("xdot");
	dependencies.push_back("gvplugin_dot_layout");
	dependencies.push_back("gvplugin_core");
	dependencies.push_back("json");
	return dependencies;
}

/**
 * Returns the file name of the main function can accompany the Vuo runtime.
 */
string VuoCompiler::getRuntimeMainDependency(void)
{
	return "VuoRuntimeMain.bc";
}

/**
 * Returns the file name of the Vuo runtime.
 */
string VuoCompiler::getRuntimeDependency(void)
{
	return "VuoRuntime.bc";
}

/**
 * Returns the path to the Clang binary.
 */
llvm::sys::Path VuoCompiler::getClangPath(void)
{
	return clangPath;
}


/**
 * Returns the path to Vuo.framework, or an empty path if the framework cannot be located.
 */
llvm::sys::Path VuoCompiler::getVuoFrameworkPath()
{
	// Partially cloned in VuoWindow.cc's VuoWindowGetVuoFrameworkPath().

	// First check whether Vuo.framework is in the list of loaded dynamic libraries.
	for(unsigned int i=0; i<_dyld_image_count(); ++i)
	{
		const char *name = _dyld_get_image_name(i);
		const char *frameworkName = "Vuo.framework/Versions/" VUO_VERSION_STRING "/Vuo";
		if(strcmp(name+strlen(name)-strlen(frameworkName),frameworkName)==0)
		{
			llvm::sys::Path path = llvm::sys::Path(StringRef(name));
			path.eraseComponent(); // remove "Vuo"
			path.eraseComponent(); // remove VUO_VERSION_STRING
			path.eraseComponent(); // remove "Versions"
			if (path.canRead())
				return path;
		}
	}


	// Failing that, check for a Vuo.framework bundled with the executable app.
	char executablePath[PATH_MAX + 1];
	uint32_t size = sizeof(executablePath);

	if (! _NSGetExecutablePath(executablePath, &size))
	{
		char cleanedExecutablePath[PATH_MAX + 1];

		// Remove extra references (e.g., "/./") from the path before manipulating.
		realpath(executablePath, cleanedExecutablePath);
		llvm::sys::Path bundledVuoFrameworkDir = llvm::sys::Path(StringRef(cleanedExecutablePath));
		bundledVuoFrameworkDir.eraseComponent(); // remove executable name from path
		bundledVuoFrameworkDir.eraseComponent(); // remove "MacOS" subdirectory from path
		bundledVuoFrameworkDir.appendComponent("Frameworks");
		bundledVuoFrameworkDir.appendComponent("Vuo.framework");

		if (bundledVuoFrameworkDir.canRead())
			return bundledVuoFrameworkDir;
	}

	// Failing that, check for ~/Library/Frameworks/Vuo.framework
	llvm::sys::Path userVuoLibraryDir = llvm::sys::Path::GetUserHomeDirectory();
	userVuoLibraryDir.appendComponent("Library");
	userVuoLibraryDir.appendComponent("Frameworks");
	userVuoLibraryDir.appendComponent("Vuo.framework");

	if (userVuoLibraryDir.canRead())
		return userVuoLibraryDir;

	// Failing that, check for /Library/Frameworks/Vuo.framework
	llvm::sys::Path systemVuoLibraryDir = llvm::sys::Path("/Library/Frameworks/Vuo.framework");

	if (systemVuoLibraryDir.canRead())
		return systemVuoLibraryDir;

	// Give up.
	return llvm::sys::Path("");
}

/**
 * Returns the filesystem path to the user-specific Vuo Modules folder.
 */
string VuoCompiler::getUserModulesPath()
{
	return sys::Path::GetUserHomeDirectory().str() + "/Library/Application Support/Vuo/Modules/";
}

/**
 * Returns the filesystem path to the system-wide Vuo Modules folder.
 */
string VuoCompiler::getSystemModulesPath()
{
	return "/Library/Application Support/Vuo/Modules/";
}

/**
 * Returns the name of the node class that would be located at @c path.
 */
string VuoCompiler::getModuleNameForPath(string path)
{
	string dir, file, extension;
	VuoFileUtilities::splitPath(path, dir, file, extension);
	return file;
}

/**
 * Adds a module search path (for node classes, types, and libraries) to use when linking a composition.
 */
void VuoCompiler::addModuleSearchPath(string path)
{
	moduleSearchPaths.push_back(path);
	addLibrarySearchPath(path);
}

/**
 * Adds a header search path to use when compiling a node class.
 */
void VuoCompiler::addHeaderSearchPath(const string &path)
{
	headerSearchPaths.push_back(path);
}

/**
 * Adds a library search path to use when linking a composition.
 */
void VuoCompiler::addLibrarySearchPath(const string &path)
{
	librarySearchPaths.push_back(path);
}

/**
 * Adds a preferred library search path to use when linking a composition.
 * Preferred paths will be searched before any other library paths.
 */
void VuoCompiler::addPreferredLibrarySearchPath(const string &path)
{
	preferredLibrarySearchPaths.push_back(path);
}

/**
 * Clears the list of preferred library paths.
 */
void VuoCompiler::clearPreferredLibrarySearchPaths(void)
{
	preferredLibrarySearchPaths.clear();
}

/**
 * Adds a Mac OS X framework search path to use when linking a composition.
 */
void VuoCompiler::addFrameworkSearchPath(const string &path)
{
	frameworkSearchPaths.push_back(path);
}

/**
 * Adds a preferred Mac OS X framework search path to use when linking a composition.
 * Preferred paths will be searched before any other framework paths.
 */
void VuoCompiler::addPreferredFrameworkSearchPath(const string &path)
{
	preferredFrameworkSearchPaths.push_back(path);
}

/**
 * Sets the telemetry option to use when compiling a composition. Valid values are "on" and "console".
 */
void VuoCompiler::setTelemetry(const string &telemetry)
{
	this->telemetry = telemetry;
}

/**
 * Sets the target triple to use when compiling or linking.
 */
void VuoCompiler::setTarget(const string &target)
{
	this->target = target;
}

/**
 * Sets the verbosity to use when compiling or linking. If true, prints some debug info and passes the `-v` option to Clang.
 */
void VuoCompiler::setVerbose(bool isVerbose)
{
	this->isVerbose = isVerbose;
}

/**
 * Sets the path to the clang binary.
 */
void VuoCompiler::setClangPath(const string &clangPath)
{
	this->clangPath = llvm::sys::Path(StringRef(clangPath));
}

/**
 * Returns the path to the VuoCompositionLoader executable.
 */
string VuoCompiler::getCompositionLoaderPath(void)
{
	string vuoFrameworkPath = getVuoFrameworkPath().str();
	return (vuoFrameworkPath.empty() ?
				VUO_ROOT "/runtime/VuoCompositionLoader" :
				vuoFrameworkPath + "/Frameworks/VuoRuntime.framework/VuoCompositionLoader");
}

/**
 * Returns the path to the VuoCompositionStub dynamic library.
 */
string VuoCompiler::getCompositionStubPath(void)
{
	string vuoFrameworkPath = getVuoFrameworkPath().str();
	return (vuoFrameworkPath.empty() ?
				VUO_ROOT "/base/VuoCompositionStub.dylib" :
				vuoFrameworkPath + "/Frameworks/VuoRuntime.framework/VuoCompositionStub.dylib");
}

/**
 * Prints info about this compiler, for debugging.
 */
void VuoCompiler::print(void)
{
	fprintf(stderr, "Module (node class, type, library) search paths:\n");
	for (vector<string>::iterator i = moduleSearchPaths.begin(); i != moduleSearchPaths.end(); ++i)
		fprintf(stderr, " %s\n", (*i).c_str());
	fprintf(stderr, "Header search paths:\n");
	for (vector<string>::iterator i = headerSearchPaths.begin(); i != headerSearchPaths.end(); ++i)
		fprintf(stderr, " %s\n", (*i).c_str());
	fprintf(stderr, "Preferred library search paths:\n");
	for (vector<string>::iterator i = preferredLibrarySearchPaths.begin(); i != preferredLibrarySearchPaths.end(); ++i)
		fprintf(stderr, " %s\n", (*i).c_str());
	fprintf(stderr, "Other library search paths:\n");
	for (vector<string>::iterator i = librarySearchPaths.begin(); i != librarySearchPaths.end(); ++i)
		fprintf(stderr, " %s\n", (*i).c_str());
	fprintf(stderr, "Preferred framework search paths:\n");
	for (vector<string>::iterator i = preferredFrameworkSearchPaths.begin(); i != preferredFrameworkSearchPaths.end(); ++i)
		fprintf(stderr, " %s\n", (*i).c_str());
	fprintf(stderr, "Other framework search paths:\n");
	for (vector<string>::iterator i = frameworkSearchPaths.begin(); i != frameworkSearchPaths.end(); ++i)
		fprintf(stderr, " %s\n", (*i).c_str());
	fprintf(stderr, "Framework path:\n");
	if (! getVuoFrameworkPath().str().empty())
		fprintf(stderr, " %s\n", getVuoFrameworkPath().c_str());
	fprintf(stderr, "Clang path:\n");
	if (! getClangPath().str().empty())
		fprintf(stderr, " %s\n", getClangPath().c_str());
}

/**
 * Creates a runner object that can run the composition in file @a compositionFilePath in a new process.
 */
VuoRunner * VuoCompiler::newSeparateProcessRunnerFromCompositionFile(string compositionFilePath)
{
	VuoCompiler compiler;
	string directory, file, extension;
	VuoFileUtilities::splitPath(compositionFilePath, directory, file, extension);
	string compiledCompositionPath = VuoFileUtilities::makeTmpFile(file, "bc");
	string linkedCompositionPath = VuoFileUtilities::makeTmpFile(file + "-linked", "");
	compiler.compileComposition(compositionFilePath, compiledCompositionPath);
	compiler.linkCompositionToCreateExecutable(compiledCompositionPath, linkedCompositionPath);
	remove(compiledCompositionPath.c_str());
	return VuoRunner::newSeparateProcessRunnerFromExecutable(linkedCompositionPath, true);
}

/**
 * Creates a runner object that can run the composition in string @a composition in a new process.
 */
VuoRunner * VuoCompiler::newSeparateProcessRunnerFromCompositionString(string composition)
{
	VuoCompiler compiler;
	string compiledCompositionPath = VuoFileUtilities::makeTmpFile("VuoRunnerComposition", "bc");
	string linkedCompositionPath = VuoFileUtilities::makeTmpFile("VuoRunnerComposition-linked", "");
	compiler.compileCompositionString(composition, compiledCompositionPath);
	compiler.linkCompositionToCreateExecutable(compiledCompositionPath, linkedCompositionPath);
	remove(compiledCompositionPath.c_str());
	return VuoRunner::newSeparateProcessRunnerFromExecutable(linkedCompositionPath, true);
}

/**
 * Creates a runner object that can run the composition in file @a compositionFilePath in this process.
 */
VuoRunner * VuoCompiler::newCurrentProcessRunnerFromCompositionFile(string compositionFilePath)
{
	VuoCompiler compiler;
	string directory, file, extension;
	VuoFileUtilities::splitPath(compositionFilePath, directory, file, extension);
	string compiledCompositionPath = VuoFileUtilities::makeTmpFile(file, "bc");
	compiler.compileComposition(compositionFilePath, compiledCompositionPath);
	string linkedCompositionPath = VuoFileUtilities::makeTmpFile(file, "dylib");
	compiler.linkCompositionToCreateDynamicLibrary(compiledCompositionPath, linkedCompositionPath);
	remove(compiledCompositionPath.c_str());
	return VuoRunner::newCurrentProcessRunnerFromDynamicLibrary(linkedCompositionPath, true);
}

/**
 * Creates a runner object that can run the composition in string @a composition in this process.
 */
VuoRunner * VuoCompiler::newCurrentProcessRunnerFromCompositionString(string composition)
{
	VuoCompiler compiler;
	string compiledCompositionPath = VuoFileUtilities::makeTmpFile("VuoRunnerComposition", "bc");
	compiler.compileCompositionString(composition, compiledCompositionPath);
	string linkedCompositionPath = VuoFileUtilities::makeTmpFile("VuoRunnerComposition", "dylib");
	compiler.linkCompositionToCreateDynamicLibrary(compiledCompositionPath, linkedCompositionPath);
	remove(compiledCompositionPath.c_str());
	return VuoRunner::newCurrentProcessRunnerFromDynamicLibrary(linkedCompositionPath, true);
}