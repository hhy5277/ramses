//  -------------------------------------------------------------------------
//  Copyright (C) 2014 BMW Car IT GmbH
//  -------------------------------------------------------------------------
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at https://mozilla.org/MPL/2.0/.
//  -------------------------------------------------------------------------

// These includes are needed by the tests
#include "RendererLifecycleTests.h"
#include "RendererTestInstance.h"
#include "RendererTestEventHandler.h"
#include "TimeoutRunnable.h"
#include "RendererTestsFramework.h"
#include "ReadPixelCallbackHandler.h"
#include "Utils/FileUtils.h"
#include "TestScenes/MultipleTrianglesScene.h"
#include "TestScenes/TextureBufferScene.h"
#include "TestScenes/DataBufferScene.h"
#include "TestScenes/TextScene.h"
#include "TestScenes/FileLoadingScene.h"
#include "TestScenes/Texture2DFormatScene.h"
#include "RendererTestUtils.h"

// These includes are needed because of ramses API usage
#include "ramses-renderer-api/IRendererEventHandler.h"
#include "ramses-renderer-api/DisplayConfig.h"
#include "ramses-client-api/DataInt32.h"

// These includes should not be needed... Technical debts + usage of internal classes
#include "RamsesRendererImpl.h"
#include "DisplayConfigImpl.h"
#include "RendererAPI/IDevice.h"
#include "RendererAPI/IRenderBackend.h"
#include "RendererAPI/ISurface.h"

#include "Common/Cpp11Macros.h"
#include <thread>
#include <iostream>

#define ASSERT_TRUE(expression) \
if (!(expression)) \
{ \
    std::cout << "!!! Failed assertion: " << #expression << " (" << __FILE__ << ":" << __LINE__ << ")" << "\n"; \
    std::cerr << "!!! Failed assertion: " << #expression << " (" << __FILE__ << ":" << __LINE__ << ")" << "\n"; \
    return false; \
} \
else \
    std::cout << "Passed assertion: " << #expression << " (" << __FILE__ << ":" << __LINE__ << ")" << "\n"; \

#define ASSERT_FALSE(expression) ASSERT_TRUE((!(expression)))

namespace ramses_internal
{
    RendererLifecycleTests::RendererLifecycleTests(const ramses_internal::StringVector& filterIn, const ramses_internal::StringVector& filterOut, int32_t argc, const char* argv[])
        : m_filterIn(filterIn)
        , m_filterOut(filterOut)
        , m_frameworkConfig(argc, argv)
    {
    }

    bool RendererLifecycleTests::isTestFiltered(ELifecycleTest test) const
    {
        const bool processFilterIn = !m_filterIn.empty();
        const bool processFilterOut = !m_filterOut.empty();

        if (!processFilterIn && !processFilterOut)
        {
            return false;
        }

        const ramses_internal::String testCaseName(EnumToString(test));

        const bool excludedByFilterIn = processFilterIn && !RendererTestsFramework::NameMatchesFilter(testCaseName, m_filterIn);
        const bool excludedByFilterOut = processFilterOut && RendererTestsFramework::NameMatchesFilter(testCaseName, m_filterOut);

        return excludedByFilterIn || excludedByFilterOut;
    }

    bool RendererLifecycleTests::runTests()
    {
        StringVector passedTests;
        StringVector failedTests;
        StringVector filteredTests;

        for (UInt32 i = 0; i < ELifecycleTest_NUMBER_OF_ELEMENTS; ++i)
        {
            const ELifecycleTest testId = static_cast<ELifecycleTest>(i);
            if (isTestFiltered(testId))
            {
                m_testReport << "Renderer lifecycle test " << EnumToString(testId) << " skipped because of filter\n";
                filteredTests.push_back(EnumToString(testId));
            }
            else
            {
                printf("======\n");
                printf("=== Running renderer lifecycle test %s ===\n", EnumToString(testId));
                printf("======\n");
                m_testReport << "Running renderer lifecycle test " << EnumToString(testId) << "...\n";
                const bool testResult = runTest(testId);
                if (testResult)
                {
                    m_testReport << "Renderer lifecycle test " << EnumToString(testId) << " PASSED!\n\n";
                    passedTests.push_back(EnumToString(testId));
                }
                else
                {
                    m_testReport << "Renderer lifecycle test " << EnumToString(testId) << " FAILED!\n\n";
                    failedTests.push_back(EnumToString(testId));
                }
            }
        }

        m_testReport << "\n\n--- Renderer lifecycle test report begin ---\n";
        {
            m_testReport << "\n  Passed renderer lifecycle test cases: " << passedTests.size();
            ramses_foreach(passedTests, testCase)
            {
                m_testReport << "\n    " << *testCase;
            }

            m_testReport << "\n\n  Failed renderer lifecycle test cases: " << failedTests.size();
            ramses_foreach(failedTests, testCase)
            {
                m_testReport << "\n    " << *testCase;
            }

            m_testReport << "\n\n  Renderer lifecycle test cases filtered out: " << filteredTests.size();
            ramses_foreach(filteredTests, testCase)
            {
                m_testReport << "\n    " << *testCase;
            }

            if (failedTests.empty())
            {
                m_testReport << "\n";
                m_testReport << "\n  ------------------";
                m_testReport << "\n  --- ALL PASSED ---";
                m_testReport << "\n  ------------------";
            }
            else
            {
                m_testReport << "\n";
                m_testReport << "\n  !!!!!!!!!!!!!!!!!!!!";
                m_testReport << "\n  !!! FAILED TESTS !!!";
                m_testReport << "\n  !!!!!!!!!!!!!!!!!!!!";
            }
        }
        m_testReport << "\n\n--- End of renderer lifecycle test report ---\n\n";

        const bool allTestsSucceeded = failedTests.empty();
        return allTestsSucceeded;
    }

    bool RendererLifecycleTests::runTest(ELifecycleTest testId)
    {
        RendererTestInstance testRenderer(m_frameworkConfig);

        switch (testId)
        {
        case ELifecycleTest_RenderScene:
        {
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(display != ramses::InvalidDisplayId);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_RecreateSceneWithSameId:
        {
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.getScenesRegistry().destroyScene(sceneId);
            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererDisplays_Black"));

            testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, sceneId, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_SaveLoadSceneFromFileThenRender:
        {
            const ramses::sceneId_t sceneId = 1234u;

            testRenderer.getScenesRegistry().createFileLoadingScene(sceneId, ramses_internal::Vector3(0.0f, 0.0f, 5.0f), m_frameworkConfig, ramses_internal::FileLoadingScene::CREATE_SAVE_DESTROY_LOAD_USING_SEPARATE_CLIENT);

            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);

            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_AfterLoadSave"));

            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_SaveLoadSceneFromFileThenRender_Threaded:
        {
            const ramses::sceneId_t sceneId = 1234u;

            testRenderer.getScenesRegistry().createFileLoadingScene(sceneId, ramses_internal::Vector3(0.0f, 0.0f, 5.0f), m_frameworkConfig, ramses_internal::FileLoadingScene::CREATE_SAVE_DESTROY_LOAD_USING_SEPARATE_CLIENT);
            if (ramses::StatusOK != testRenderer.validateScene(sceneId))
            {
                m_testReport << "Scene not valid after loading from file!\n";
                m_testReport << testRenderer.getValidationReport(sceneId);
                return false;
            }

            testRenderer.initializeRenderer();
            testRenderer.startRendererThread();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);

            // Subscribe
            testRenderer.subscribeScene(sceneId);

            // Map
            testRenderer.mapScene(display, sceneId);

            testRenderer.flush(sceneId, 1);
            testRenderer.waitForNamedFlush(sceneId, 1);

            // Show
            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_AfterLoadSave"));

            testRenderer.hideScene(sceneId);
            testRenderer.unmapScene(sceneId);

            testRenderer.unpublish(sceneId);
            testRenderer.destroyDisplay(display);
            testRenderer.stopRendererThread();
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_DestroyAndRecreateRenderer:
        {
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES,
                ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.initializeRenderer();
            ramses::displayId_t display = createDisplayForWindow(testRenderer);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            testRenderer.initializeRenderer();
            display = createDisplayForWindow(testRenderer);

            testRenderer.publish(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_DestroyRenderer_ChangeScene_ThenRecreateRenderer:
        {
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::TextScene>(ramses_internal::TextScene::EState_INITIAL);
            testRenderer.initializeRenderer();
            ramses::displayId_t display = createDisplayForWindow(testRenderer);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            testRenderer.getScenesRegistry().setSceneState<ramses_internal::TextScene>(sceneId, ramses_internal::TextScene::EState_INITIAL_128_BY_64_VIEWPORT);

            testRenderer.initializeRenderer();
            display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_SimpleText"));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_UnsubscribeRenderer_ChangeScene_ThenResubscribeRenderer:
        {
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.hideUnmapAndUnsubscribeScene(sceneId);
            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererDisplays_Black"));

            testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Triangles_reordered"));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_ChangeScene_UnsubscribeRenderer_Flush_ThenResubscribeRenderer:
        {
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(display != ramses::InvalidDisplayId);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
            testRenderer.hideUnmapAndUnsubscribeScene(sceneId);
            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererDisplays_Black"));

            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Triangles_reordered"));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_RAMSES2881_CreateRendererAfterScene:
        {
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES,
                ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.initializeRenderer();
            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);

            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_DestroyDisplayAndRemapSceneToOtherDisplay:
        {
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES,
                ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.initializeRenderer();
            const ramses::displayId_t display1 = createDisplayForWindow(testRenderer, 0u);
            const ramses::displayId_t display2 = createDisplayForWindow(testRenderer, 1u);
            ASSERT_TRUE(ramses::InvalidDisplayId != display1);
            ASSERT_TRUE(ramses::InvalidDisplayId != display2);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display1, sceneId);
            testRenderer.showScene(sceneId);

            testRenderer.hideScene(sceneId);
            testRenderer.unmapScene(sceneId);

            testRenderer.destroyDisplay(display1);

            testRenderer.mapScene(display2, sceneId);
            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display2, "ARendererInstance_Three_Triangles"));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_RenderScene_Threaded:
        {
            testRenderer.initializeRenderer();
            testRenderer.startRendererThread();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);

            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);

            testRenderer.flush(sceneId, 1u);
            testRenderer.waitForNamedFlush(sceneId, 1u);

            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.hideScene(sceneId);
            testRenderer.unmapScene(sceneId);

            testRenderer.unpublish(sceneId);
            testRenderer.destroyDisplay(display);
            testRenderer.stopRendererThread();
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_RenderChangingScene_Threaded:
        {
            testRenderer.initializeRenderer();
            testRenderer.startRendererThread();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));

            // create data to change
            ramses::DataInt32* data = testRenderer.getScenesRegistry().getScene(sceneId).createDataInt32();
            testRenderer.flush(sceneId);

            testRenderer.publish(sceneId);

            //do not wait for subscription
            testRenderer.subscribeScene(sceneId, false);

            // change scene while subscription is ongoing
            for (int i = 0; i < 1000; i++)
            {
                data->setValue(i);
                testRenderer.flush(sceneId);
            }
            testRenderer.waitForSubscription(sceneId);

            testRenderer.mapScene(display, sceneId);

            testRenderer.flush(sceneId, 1u);
            testRenderer.waitForNamedFlush(sceneId, 1u);

            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.hideScene(sceneId);
            testRenderer.unmapScene(sceneId);

            testRenderer.unpublish(sceneId);
            testRenderer.destroyDisplay(display);
            testRenderer.stopRendererThread();
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_RenderScene_StartStopThreadMultipleTimes:
        {
            testRenderer.initializeRenderer();
            testRenderer.startRendererThread();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.publish(sceneId);

            testRenderer.subscribeScene(sceneId, false);
            testRenderer.flush(sceneId);
            testRenderer.waitForSubscription(sceneId);

            testRenderer.mapScene(display, sceneId);

            testRenderer.flush(sceneId, 1u);
            testRenderer.waitForNamedFlush(sceneId, 1u);

            testRenderer.showScene(sceneId);

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.stopRendererThread();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            testRenderer.startRendererThread();

            testRenderer.stopRendererThread();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            testRenderer.startRendererThread();

            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.hideScene(sceneId);
            testRenderer.unmapScene(sceneId);

            testRenderer.unpublish(sceneId);
            testRenderer.destroyDisplay(display);
            testRenderer.stopRendererThread();
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_DestroyRendererWhileThreadRunning:
        {
            testRenderer.initializeRenderer();
            testRenderer.startRendererThread();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.publish(sceneId);

            testRenderer.subscribeScene(sceneId, false);
            testRenderer.flush(sceneId);
            testRenderer.waitForSubscription(sceneId);

            testRenderer.mapScene(display, sceneId);

            testRenderer.flush(sceneId, 1u);
            testRenderer.waitForNamedFlush(sceneId, 1u);

            testRenderer.showScene(sceneId);

            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_RendererUploadsResourcesIfIviSurfaceInvisible:
        {
            testRenderer.initializeRenderer();
            testRenderer.startRendererThread();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer, 0u, false);
            ASSERT_TRUE(display != ramses::InvalidDisplayId);

            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::Texture2DFormatScene>(ramses_internal::Texture2DFormatScene::EState_R8, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId); // this would time out if resources for the scene could not be uploaded

            testRenderer.unmapScene(sceneId);
            testRenderer.unpublish(sceneId);
            testRenderer.destroyDisplay(display);
            testRenderer.stopRendererThread();
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_RendererUploadsResourcesIfIviSurfaceInvisibleInLoopModeUpdateOnly:
        {
            testRenderer.initializeRenderer();
            testRenderer.setLoopMode(ramses::ELoopMode_UpdateOnly);
            testRenderer.startRendererThread();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer, 0u, false);
            ASSERT_TRUE(display != ramses::InvalidDisplayId);

            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::Texture2DFormatScene>(ramses_internal::Texture2DFormatScene::EState_R8, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId); // this would time out if resources for the scene could not be uploaded

            testRenderer.unmapScene(sceneId);
            testRenderer.unpublish(sceneId);
            testRenderer.destroyDisplay(display);
            testRenderer.stopRendererThread();
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_RemapScenesWithDynamicResourcesToOtherDisplay:
        {
            const ramses::sceneId_t sceneId1 = testRenderer.getScenesRegistry().createScene<TextureBufferScene>(TextureBufferScene::EState_RGBA8_OneMip_ScaledDown, Vector3(-0.1f, -0.1f, 15.0f));
            const ramses::sceneId_t sceneId2 = testRenderer.getScenesRegistry().createScene<DataBufferScene>(DataBufferScene::INDEX_DATA_BUFFER_UINT16, Vector3(-2.0f, -2.0f, 15.0f));
            testRenderer.initializeRenderer();
            const ramses::displayId_t display1 = createDisplayForWindow(testRenderer, 0u);
            const ramses::displayId_t display2 = createDisplayForWindow(testRenderer, 1u);
            ASSERT_TRUE(ramses::InvalidDisplayId != display1);
            ASSERT_TRUE(ramses::InvalidDisplayId != display2);

            testRenderer.publish(sceneId1);
            testRenderer.publish(sceneId2);
            testRenderer.flush(sceneId1);
            testRenderer.flush(sceneId2);
            testRenderer.subscribeScene(sceneId1);
            testRenderer.subscribeScene(sceneId2);
            testRenderer.mapScene(display1, sceneId1);
            testRenderer.mapScene(display1, sceneId2);
            testRenderer.showScene(sceneId1);
            testRenderer.showScene(sceneId2);

            testRenderer.hideScene(sceneId1);
            testRenderer.hideScene(sceneId2);
            testRenderer.unmapScene(sceneId1);
            testRenderer.unmapScene(sceneId2);

            ASSERT_TRUE(checkScreenshot(testRenderer, display1, "ARendererDisplays_Black"));

            testRenderer.mapScene(display2, sceneId1);
            testRenderer.mapScene(display2, sceneId2);
            testRenderer.showScene(sceneId1);
            testRenderer.showScene(sceneId2);

            ASSERT_TRUE(checkScreenshot(testRenderer, display2, "ARendererInstance_DynamicResources"));

            testRenderer.unpublish(sceneId1);
            testRenderer.unpublish(sceneId2);
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_SceneCanReachShownStateWithLoopModeUpdateOnly_UsingDoOneLoop:
        {
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::Texture2DFormatScene>(ramses_internal::Texture2DFormatScene::EState_R8, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.initializeRenderer();
            testRenderer.setLoopMode(ramses::ELoopMode_UpdateOnly);

            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(display != ramses::InvalidDisplayId);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_SceneCanReachShownStateWithLoopModeUpdateOnly_UsingRenderThread:
        {
            testRenderer.initializeRenderer();
            testRenderer.startRendererThread();
            testRenderer.setLoopMode(ramses::ELoopMode_UpdateOnly);

            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::Texture2DFormatScene>(ramses_internal::Texture2DFormatScene::EState_R8, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            testRenderer.hideScene(sceneId);
            testRenderer.unmapScene(sceneId);

            testRenderer.unpublish(sceneId);
            testRenderer.destroyDisplay(display);
            testRenderer.stopRendererThread();
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_SceneCanReachShownStateWithLoopModeUpdateOnly_IfIviSurfaceInvisible:
        {
            testRenderer.initializeRenderer();
            testRenderer.startRendererThread();
            testRenderer.setLoopMode(ramses::ELoopMode_UpdateOnly);

            const ramses::displayId_t display = createDisplayForWindow(testRenderer, 0u, false);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::Texture2DFormatScene>(ramses_internal::Texture2DFormatScene::EState_R8, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);

            testRenderer.hideScene(sceneId);
            testRenderer.unmapScene(sceneId);

            testRenderer.unpublish(sceneId);
            testRenderer.destroyDisplay(display);
            testRenderer.stopRendererThread();
            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_DoesNotRenderToFramebufferInLoopModeUpdateOnly:
        {
            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer, 0u);
            ASSERT_TRUE(display != ramses::InvalidDisplayId);

            testRenderer.setLoopMode(ramses::ELoopMode_UpdateOnly);
            testRenderer.readPixels(display, 0u, 0u, WindowWidth, WindowHeight);
            testRenderer.flushRenderer();
            testRenderer.doOneLoop();

            ReadPixelCallbackHandler callbackHandler;

            testRenderer.dispatchRendererEvents(callbackHandler);
            ASSERT_FALSE(callbackHandler.m_pixelDataRead);

            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_Republish_ThenChangeScene:
        {
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));
            testRenderer.initializeRenderer();
            ramses::displayId_t display = createDisplayForWindow(testRenderer);

            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Three_Triangles"));

            testRenderer.unpublish(sceneId);
            testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);

            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            ASSERT_TRUE(checkScreenshot(testRenderer, display, "ARendererInstance_Triangles_reordered"));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_CreateDisplayWithBiggerDimensionThanPhysicalDisplay:
        {
            testRenderer.initializeRenderer();

            ramses::DisplayConfig displayConfig = RendererTestUtils::CreateTestDisplayConfig(0u);
            const UInt32 windowWidth = 30u;
            const UInt32 windowHeight = 2000; //set very big height that is bigger than physical display width
            displayConfig.setWindowRectangle(100u, 200u, windowWidth, windowHeight);
            displayConfig.setOrthographicProjection(-3.f, 3.f, -3.f, 3.f, 0.1f, 10000.f);
            const ramses::displayId_t display = testRenderer.createDisplay(displayConfig);

            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(-0.50f, 1.0f, 5.0f));
            testRenderer.publish(sceneId);
            testRenderer.flush(sceneId);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            testRenderer.doOneLoop();
            ASSERT_TRUE(testRenderer.performScreenshotCheck(display, 0u, 0u, windowWidth, windowHeight, "ARendererInstance_Three_Triangles_2000x30"));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_PollingFrameCallbacks_DoesNotBlockIfNoDisplaysExist:
        {
            const std::chrono::seconds largePollingTime{100u};
            RendererTestUtils::SetMaxFrameCallbackPollingTime(std::chrono::duration_cast<std::chrono::microseconds>(largePollingTime));

            testRenderer.initializeRenderer();

            const auto startTime = std::chrono::steady_clock::now();

            testRenderer.flushRenderer();
            testRenderer.doOneLoop();
            testRenderer.flushRenderer();
            testRenderer.doOneLoop();
            testRenderer.flushRenderer();
            testRenderer.doOneLoop();
            testRenderer.flushRenderer();
            testRenderer.doOneLoop();

            const auto timeElapsed  = std::chrono::steady_clock::now() - startTime;
            const std::chrono::milliseconds maximumExpectedTime{ largePollingTime/ 2u };
            ASSERT_TRUE(timeElapsed < maximumExpectedTime);

            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_PollingFrameCallbacks_BlocksIfDisplayNotReadyToRender:
        {
            const std::chrono::milliseconds nonTrivialPollingTime{50u};
            RendererTestUtils::SetMaxFrameCallbackPollingTime(std::chrono::duration_cast<std::chrono::microseconds>(nonTrivialPollingTime));

            testRenderer.initializeRenderer();

            if(testRenderer.hasSystemCompositorController())
            {
                createDisplayForWindow(testRenderer, 0u, false);

                const auto startTime = std::chrono::steady_clock::now();

                testRenderer.flushRenderer();
                testRenderer.doOneLoop();
                testRenderer.flushRenderer();
                testRenderer.doOneLoop();

                const auto timeElapsed  = std::chrono::steady_clock::now() - startTime;
                ASSERT_TRUE(timeElapsed >= nonTrivialPollingTime);
            }

            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_PollingFrameCallbacks_BlocksIfAllDisplaysNotReadyToRender:
        {
            const std::chrono::milliseconds nonTrivialPollingTime{50u};
            RendererTestUtils::SetMaxFrameCallbackPollingTime(std::chrono::duration_cast<std::chrono::microseconds>(nonTrivialPollingTime));

            testRenderer.initializeRenderer();

            if(testRenderer.hasSystemCompositorController())
            {
                createDisplayForWindow(testRenderer, 0u, false);
                createDisplayForWindow(testRenderer, 1u, false);

                const auto startTime = std::chrono::steady_clock::now();

                testRenderer.flushRenderer();
                testRenderer.doOneLoop();
                testRenderer.flushRenderer();
                testRenderer.doOneLoop();

                const auto timeElapsed  = std::chrono::steady_clock::now() - startTime;
                ASSERT_TRUE(timeElapsed >= nonTrivialPollingTime);
            }

            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_PollingFrameCallbacks_UnreadyDisplayDoesNotBlockReadyDisplay:
        {
            const std::chrono::seconds largePollingTime{100u};
            RendererTestUtils::SetMaxFrameCallbackPollingTime(std::chrono::duration_cast<std::chrono::microseconds>(largePollingTime));

            testRenderer.initializeRenderer();

            if(testRenderer.hasSystemCompositorController())
            {
                const ramses::displayId_t display1 = createDisplayForWindow(testRenderer, 0u, true);
                createDisplayForWindow(testRenderer, 1u, false);

                const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));

                testRenderer.publish(sceneId);
                testRenderer.flush(sceneId);
                testRenderer.subscribeScene(sceneId);
                testRenderer.mapScene(display1, sceneId);
                testRenderer.showScene(sceneId);

                const auto startTime = std::chrono::steady_clock::now();

                testRenderer.flushRenderer();
                testRenderer.doOneLoop();

                testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
                testRenderer.flush(sceneId);
                ASSERT_TRUE(checkScreenshot(testRenderer, display1, "ARendererInstance_Triangles_reordered"));

                const auto timeElapsed  = std::chrono::steady_clock::now() - startTime;

                const auto maximumExpectedTime = largePollingTime / 2;
                ASSERT_TRUE(timeElapsed < maximumExpectedTime);
            }

            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_PollingFrameCallbacks_UnreadyDisplayDoesNotBlockReadyDisplay_DisplaysInOtherOrder:
        {
            const std::chrono::seconds largePollingTime{100u};
            RendererTestUtils::SetMaxFrameCallbackPollingTime(std::chrono::duration_cast<std::chrono::microseconds>(largePollingTime));

            testRenderer.initializeRenderer();

            if(testRenderer.hasSystemCompositorController())
            {
                createDisplayForWindow(testRenderer, 0u, false);
                const ramses::displayId_t display2 = createDisplayForWindow(testRenderer, 1u, true);

                const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));

                testRenderer.publish(sceneId);
                testRenderer.flush(sceneId);
                testRenderer.subscribeScene(sceneId);
                testRenderer.mapScene(display2, sceneId);
                testRenderer.showScene(sceneId);

                const auto startTime = std::chrono::steady_clock::now();

                testRenderer.flushRenderer();
                testRenderer.doOneLoop();

                testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
                testRenderer.flush(sceneId);
                ASSERT_TRUE(checkScreenshot(testRenderer, display2, "ARendererInstance_Triangles_reordered"));

                const auto timeElapsed  = std::chrono::steady_clock::now() - startTime;
                const auto maximumExpectedTime = largePollingTime / 2;
                ASSERT_TRUE(timeElapsed < maximumExpectedTime);
            }

            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_PollingFrameCallbacks_ReadyDisplayDoesNotStarveOtherDisplay:
        {
            const std::chrono::milliseconds nonTrivialPollingTime{50u};
            RendererTestUtils::SetMaxFrameCallbackPollingTime(std::chrono::duration_cast<std::chrono::microseconds>(nonTrivialPollingTime));

            testRenderer.initializeRenderer();

            if(testRenderer.hasSystemCompositorController())
            {
                const ramses::displayId_t display1 = createDisplayForWindow(testRenderer, 0u, true);
                createDisplayForWindow(testRenderer, 1u, true); //nothing gets rendered on it, so it is ALWAYS ready (except right after clearing)

                ASSERT_TRUE(checkScreenshot(testRenderer, display1, "ARendererDisplays_Black"));

                const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));

                testRenderer.publish(sceneId);
                testRenderer.flush(sceneId);
                testRenderer.subscribeScene(sceneId);
                testRenderer.mapScene(display1, sceneId);
                testRenderer.showScene(sceneId);

                ASSERT_TRUE(checkScreenshot(testRenderer, display1, "ARendererInstance_Three_Triangles"));

                //render again and make sure the display was updated
                testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
                testRenderer.flush(sceneId);
                //taking the screenshot would timeout if display1 is being starved by the (always ready) other display
                ASSERT_TRUE(checkScreenshot(testRenderer, display1, "ARendererInstance_Triangles_reordered"));
            }

            testRenderer.destroyRenderer();
            return true;
        }
        case ELifecycleTest_PollingFrameCallbacks_ReadyDisplayDoesNotStarveOtherDisplay_DisplaysInOtherOrder:
        {
            const std::chrono::milliseconds nonTrivialPollingTime{50u};
            RendererTestUtils::SetMaxFrameCallbackPollingTime(std::chrono::duration_cast<std::chrono::microseconds>(nonTrivialPollingTime));

            testRenderer.initializeRenderer();

            if(testRenderer.hasSystemCompositorController())
            {
                createDisplayForWindow(testRenderer, 0u, true); //nothing gets rendered on it, so it is ALWAYS ready (except right after clearing)
                const ramses::displayId_t display2 = createDisplayForWindow(testRenderer, 1u, true);

                ASSERT_TRUE(checkScreenshot(testRenderer, display2, "ARendererDisplays_Black"));

                const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(0.0f, 0.0f, 5.0f));

                testRenderer.publish(sceneId);
                testRenderer.flush(sceneId);
                testRenderer.subscribeScene(sceneId);
                testRenderer.mapScene(display2, sceneId);
                testRenderer.showScene(sceneId);

                ASSERT_TRUE(checkScreenshot(testRenderer, display2, "ARendererInstance_Three_Triangles"));

                //render again and make sure the display was updated
                testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
                testRenderer.flush(sceneId);
                //taking the screenshot would timeout if display1 is being starved by the (always ready) other display
                ASSERT_TRUE(checkScreenshot(testRenderer, display2, "ARendererInstance_Triangles_reordered"));
            }

            testRenderer.destroyRenderer();
            return true;
        }

        case ELifecycleTest_SceneLatencyNotExceededWhileSubscribed:
        {
            testRenderer.initializeRenderer();

            ramses::SceneConfig sceneConfig;
            sceneConfig.setMaximumLatency(3600 * 1000); // 1 hour limit
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(-0.50f, 1.0f, 5.0f), sceneConfig);
            testRenderer.publish(sceneId);

            const auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();

            testRenderer.flushWithTimestamp(sceneId, timeStamp);
            testRenderer.subscribeScene(sceneId);

            for (int i = 0; i < 5; ++i)
            {
                testRenderer.flushWithTimestamp(sceneId, timeStamp);
                testRenderer.doOneLoop();
            }

            ASSERT_FALSE(testRenderer.consumeEventsAndCheckScenesLatencyExceeded({ sceneId }));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_SceneLatencyExceededWhileSubscribed:
        {
            testRenderer.initializeRenderer();

            ramses::SceneConfig sceneConfig;
            sceneConfig.setMaximumLatency(3600 * 1000); // 1 hour limit
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(-0.50f, 1.0f, 5.0f), sceneConfig);
            testRenderer.publish(sceneId);

            // there has to be an initial flush before subscribing and we have to use flush with timestamp
            // use timestamp in future so that it is guaranteed we don't get latency exceed already at subscription time
            const auto timeStampInFuture = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() + std::chrono::hours(1)).time_since_epoch()).count();
            testRenderer.flushWithTimestamp(sceneId, timeStampInFuture);
            testRenderer.subscribeScene(sceneId);
            testRenderer.doOneLoop();

            // next flush will be in past to trigger the exceeded event
            const uint64_t timeStampInPast = 1u;
            testRenderer.flushWithTimestamp(sceneId, timeStampInPast);
            testRenderer.doOneLoop();

            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyExceeded({ sceneId }));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_SceneLatencyExceededAndRecoveredWhileSubscribed:
        {
            testRenderer.initializeRenderer();

            ramses::SceneConfig sceneConfig;
            sceneConfig.setMaximumLatency(3600 * 1000); // 1 hour limit
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(-0.50f, 1.0f, 5.0f), sceneConfig);
            testRenderer.publish(sceneId);

            // there has to be an initial flush before subscribing and we have to use flush with timestamp
            // use timestamp in future so that it is guaranteed we don't get latency exceed already at subscription time
            const auto timeStampInFuture = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() + std::chrono::hours(1)).time_since_epoch()).count();
            testRenderer.flushWithTimestamp(sceneId, timeStampInFuture);
            testRenderer.subscribeScene(sceneId);
            testRenderer.doOneLoop();

            // next flush will be in past to trigger the exceeded event
            const uint64_t timeStampInPast = 1u;
            testRenderer.flushWithTimestamp(sceneId, timeStampInPast);
            testRenderer.doOneLoop();
            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyExceeded({ sceneId }));

            // next flush will be in future again to trigger the recovery event
            testRenderer.flushWithTimestamp(sceneId, timeStampInFuture);
            testRenderer.doOneLoop();

            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyRecovered({ sceneId }));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_SceneLatencyNotExceededWhileRendered:
        {
            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            ramses::SceneConfig sceneConfig;
            sceneConfig.setMaximumLatency(3600 * 1000); // 1 hour limit
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(-0.50f, 1.0f, 5.0f), sceneConfig);
            testRenderer.publish(sceneId);

            // there has to be an initial flush before subscribing and we have to use flush with timestamp
            // use timestamp in future so that it is guaranteed we don't get latency exceed already at subscription time
            const auto timeStampInFuture = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() + std::chrono::milliseconds(100)).time_since_epoch()).count();
            testRenderer.flushWithTimestamp(sceneId, timeStampInFuture);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            testRenderer.doOneLoop();

            // send flushes and render within latency limit
            for (int i = 0; i < 5; ++i)
            {
                // make modifications to scene
                testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
                const auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();
                testRenderer.flushWithTimestamp(sceneId, timeStamp);
                testRenderer.doOneLoop();
            }

            ASSERT_FALSE(testRenderer.consumeEventsAndCheckScenesLatencyExceeded({ sceneId }));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_SceneLatencyNotExceededWhileRenderedButUpdatedWithEmptyFlushes:
        {
            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            ramses::SceneConfig sceneConfig;
            sceneConfig.setMaximumLatency(3600 * 1000); // 1 hour limit
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(-0.50f, 1.0f, 5.0f), sceneConfig);
            testRenderer.publish(sceneId);

            // there has to be an initial flush before subscribing and we have to use flush with timestamp
            // use timestamp in future so that it is guaranteed we don't get latency exceed already at subscription time
            const auto timeStampInFuture = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() + std::chrono::milliseconds(100)).time_since_epoch()).count();
            testRenderer.flushWithTimestamp(sceneId, timeStampInFuture);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            testRenderer.doOneLoop();

            // send flushes and render within latency limit
            for (int i = 0; i < 5; ++i)
            {
                // no modifications to scene
                const auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();
                testRenderer.flushWithTimestamp(sceneId, timeStamp);
                testRenderer.doOneLoop();
            }

            ASSERT_FALSE(testRenderer.consumeEventsAndCheckScenesLatencyExceeded({ sceneId }));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_SceneLatencyExceededWhileRendered:
        {
            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            ramses::SceneConfig sceneConfig;
            sceneConfig.setMaximumLatency(100);
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(-0.50f, 1.0f, 5.0f), sceneConfig);
            testRenderer.publish(sceneId);

            // there has to be an initial flush before subscribing and we have to use flush with timestamp
            // use timestamp in future so that it is guaranteed we don't get latency exceed already at subscription time
            const auto timeStampInFuture = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() + std::chrono::milliseconds(1000)).time_since_epoch()).count();
            testRenderer.flushWithTimestamp(sceneId, timeStampInFuture);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            testRenderer.doOneLoop();

            // send flushes within latency limit but do not render
            testRenderer.setLoopMode(ramses::ELoopMode_UpdateOnly);
            for (int i = 0; i < 5; ++i)
            {
                const auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();
                // make modifications to scene
                testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
                testRenderer.flushWithTimestamp(sceneId, timeStamp);
                testRenderer.doOneLoop();
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }

            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyExceeded({ sceneId }));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_SceneLatencyExceededAndRecoveredWhileRendered:
        {
            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            ramses::SceneConfig sceneConfig;
            sceneConfig.setMaximumLatency(100);
            const ramses::sceneId_t sceneId = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(-0.50f, 1.0f, 5.0f), sceneConfig);
            testRenderer.publish(sceneId);

            // there has to be an initial flush before subscribing and we have to use flush with timestamp
            // use timestamp in future so that it is guaranteed we don't get latency exceed already at subscription time
            const auto timeStampInFuture = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() + std::chrono::milliseconds(1000)).time_since_epoch()).count();
            testRenderer.flushWithTimestamp(sceneId, timeStampInFuture);
            testRenderer.subscribeScene(sceneId);
            testRenderer.mapScene(display, sceneId);
            testRenderer.showScene(sceneId);
            testRenderer.doOneLoop();

            // send flushes within latency limit but do not render
            testRenderer.setLoopMode(ramses::ELoopMode_UpdateOnly);
            for (int i = 0; i < 5; ++i)
            {
                const auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();
                // make modifications to scene
                testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
                testRenderer.flushWithTimestamp(sceneId, timeStamp);
                testRenderer.doOneLoop();
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyExceeded({ sceneId }));

            // now also render within latency limit to recover
            testRenderer.setLoopMode(ramses::ELoopMode_UpdateAndRender);
            for (int i = 0; i < 5; ++i)
            {
                const auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();
                // make modifications to scene
                testRenderer.getScenesRegistry().setSceneState<ramses_internal::MultipleTrianglesScene>(sceneId, ramses_internal::MultipleTrianglesScene::TRIANGLES_REORDERED);
                testRenderer.flushWithTimestamp(sceneId, timeStamp);
                testRenderer.doOneLoop();
            }
            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyRecovered({ sceneId }));

            testRenderer.unpublish(sceneId);
            testRenderer.destroyRenderer();

            return true;
        }
        case ELifecycleTest_SceneLatencyTwoScenesExceededOneAfterAnother:
        {
            testRenderer.initializeRenderer();
            const ramses::displayId_t display = createDisplayForWindow(testRenderer);
            ASSERT_TRUE(ramses::InvalidDisplayId != display);

            ramses::SceneConfig sceneConfig;
            sceneConfig.setMaximumLatency(100);
            const ramses::sceneId_t sceneId1 = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(-0.50f, 1.0f, 5.0f), sceneConfig);
            const ramses::sceneId_t sceneId2 = testRenderer.getScenesRegistry().createScene<ramses_internal::MultipleTrianglesScene>(ramses_internal::MultipleTrianglesScene::THREE_TRIANGLES, ramses_internal::Vector3(-0.50f, 1.0f, 5.0f), sceneConfig);
            testRenderer.publish(sceneId1);
            testRenderer.publish(sceneId2);

            // there has to be an initial flush before subscribing and we have to use flush with timestamp
            // use timestamp in future so that it is guaranteed we don't get latency exceed already at subscription time
            const auto timeStampInFuture = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() + std::chrono::milliseconds(1000)).time_since_epoch()).count();
            testRenderer.flushWithTimestamp(sceneId1, timeStampInFuture);
            testRenderer.flushWithTimestamp(sceneId2, timeStampInFuture);
            testRenderer.subscribeScene(sceneId1);
            testRenderer.subscribeScene(sceneId2);
            testRenderer.mapScene(display, sceneId1);
            testRenderer.mapScene(display, sceneId2);
            testRenderer.showScene(sceneId1);
            testRenderer.showScene(sceneId2);
            testRenderer.doOneLoop();

            // S1 exceeds, S2 is ok
            for (int i = 0; i < 5; ++i)
            {
                const auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();
                testRenderer.flushWithTimestamp(sceneId2, timeStamp);
                testRenderer.doOneLoop();
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyExceeded({ sceneId1 }));

            // S1 recovers, S2 is ok
            for (int i = 0; i < 5; ++i)
            {
                const auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();
                testRenderer.flushWithTimestamp(sceneId1, timeStamp);
                testRenderer.flushWithTimestamp(sceneId2, timeStamp);
                testRenderer.doOneLoop();
            }
            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyRecovered({ sceneId1 }));

            // S1 ok, S2 exceeds
            for (int i = 0; i < 5; ++i)
            {
                const auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();
                testRenderer.flushWithTimestamp(sceneId1, timeStamp);
                testRenderer.doOneLoop();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyExceeded({ sceneId2 }));

            // S1 ok, S2 is recovers
            for (int i = 0; i < 5; ++i)
            {
                const auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();
                testRenderer.flushWithTimestamp(sceneId1, timeStamp);
                testRenderer.flushWithTimestamp(sceneId2, timeStamp);
                testRenderer.doOneLoop();
            }
            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyRecovered({ sceneId2 }));

            // both S1 and S2 exceed
            for (int i = 0; i < 5; ++i)
            {
                testRenderer.doOneLoop();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            ASSERT_TRUE(testRenderer.consumeEventsAndCheckScenesLatencyExceeded({ sceneId1, sceneId2 }));

            testRenderer.unpublish(sceneId1);
            testRenderer.unpublish(sceneId2);
            testRenderer.destroyRenderer();

            return true;
        }
        default:
            assert(false);
            return false;
        }
    }

    ramses::displayId_t RendererLifecycleTests::createDisplayForWindow(RendererTestInstance& testRenderer, uint32_t iviSurfaceIdOffset, bool iviWindowStartVisible)
    {
        ramses::DisplayConfig displayConfig = RendererTestUtils::CreateTestDisplayConfig(iviSurfaceIdOffset, iviWindowStartVisible);
        displayConfig.setWindowRectangle(WindowX, WindowY, WindowWidth, WindowHeight);
        displayConfig.setPerspectiveProjection(19.f, static_cast<float>(WindowWidth) / WindowHeight, 0.1f, 1500.f);
        return testRenderer.createDisplay(displayConfig);
    }

    bool RendererLifecycleTests::checkScreenshot(RendererTestInstance& testRenderer, const ramses::displayId_t display, const char* screenshotFile)
    {
        const bool imageMatches = testRenderer.performScreenshotCheck(display, 0u, 0u, WindowWidth, WindowHeight, screenshotFile);
        if (imageMatches)
        {
            m_testReport << "    Screenshot as expected '" << screenshotFile << "'\n";
        }
        else
        {
            m_testReport << "    Screenshot test failed '" << screenshotFile << "'\n";
        }

        return imageMatches;
    }

    void RendererLifecycleTests::logReport()
    {
        printf("%s\n", m_testReport.c_str());
    }

    void RendererLifecycleTests::writeReportToFile(ramses_internal::String fileName)
    {
        ramses_internal::File file(fileName);
        ramses_internal::FileUtils::WriteAllText(file, m_testReport.c_str());
    }
}