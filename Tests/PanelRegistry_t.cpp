#include "PanelRegistry.h"
#include "doctest/doctest.h"
#include <cstddef>

namespace
{
    class FakePanel : public IEditorPanel
    {
      public:
        void Draw(GameEditor *editor) override
        {
            (void)editor;
            ++s_Draws;
        }
        static int s_Draws;
    };
    int FakePanel::s_Draws = 0;
} // namespace

TEST_CASE("PanelRegistry: extension panel self-registers and builds")
{
    const size_t before = s_ExtensionPanels().size();
    CHECK(b_RegisterPanel<FakePanel>());

    REQUIRE(s_ExtensionPanels().size() == before + 1);

    std::unique_ptr<IEditorPanel> panel = s_ExtensionPanels().back()();
    REQUIRE(panel != nullptr);
    CHECK(dynamic_cast<FakePanel *>(panel.get()) != nullptr);

    // Factories are repeatable (no double-registration hazards)
    std::unique_ptr<IEditorPanel> second = s_ExtensionPanels().back()();
    CHECK(second != nullptr);
    CHECK(second.get() != panel.get());

    // Leave global state as found for other tests
    s_ExtensionPanels().pop_back();
    CHECK(s_ExtensionPanels().size() == before);
}
