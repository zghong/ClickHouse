#include <Interpreters/ActionLocksManager.h>
#include <Storages/IStorage.h>
#include <Common/ActionBlocker.h>
#include <Common/tests/gtest_global_context.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <new>

namespace DB
{
namespace ActionLocks
{
    extern const StorageActionBlockType PartsMerge;
    extern const StorageActionBlockType PartsMove;
    extern const StorageActionBlockType ViewRefresh;
    extern const StorageActionBlockType ViewRefreshPause;
}

namespace
{

class ActionLocksTestStorage final : public IStorage
{
public:
    explicit ActionLocksTestStorage(ActionBlocker & blocker_)
        : IStorage(StorageID("test", "action_locks")), blocker(blocker_)
    {
    }

    String getName() const override
    {
        return "ActionLocksTestStorage";
    }

    ActionLock getActionLock(StorageActionBlockType) override
    {
        return blocker.cancel();
    }

private:
    ActionBlocker & blocker;
};

}

TEST(ActionLocksManager, GetStoppedActionsReturnsRegisteredControls)
{
    ActionLocksManager manager(getContext().context);
    ActionBlocker blocker;
    auto storage = std::make_shared<ActionLocksTestStorage>(blocker);

    manager.add(storage, ActionLocks::PartsMerge);
    manager.add(storage, ActionLocks::PartsMerge);
    manager.add(storage, ActionLocks::PartsMove);
    EXPECT_EQ(manager.getStoppedActions(storage), (Names{"merges", "moves"}));

    /// Reading does not disturb the registry.
    EXPECT_EQ(manager.getStoppedActions(storage), (Names{"merges", "moves"}));
    EXPECT_TRUE(blocker.isCancelled());

    manager.remove(storage, ActionLocks::PartsMerge);
    EXPECT_EQ(manager.getStoppedActions(storage), Names{"moves"});
    manager.remove(storage, ActionLocks::PartsMove);
    EXPECT_TRUE(manager.getStoppedActions(storage).empty());
    EXPECT_FALSE(blocker.isCancelled());
}

TEST(ActionLocksManager, StorageOwnershipSurvivesAddressReuse)
{
    ActionLocksManager manager(getContext().context);
    alignas(ActionLocksTestStorage) std::byte memory[sizeof(ActionLocksTestStorage)];
    auto make_storage = [&](ActionBlocker & blocker)
    {
        return StoragePtr(new (memory) ActionLocksTestStorage(blocker), [](IStorage * storage)
        {
            static_cast<ActionLocksTestStorage *>(storage)->~ActionLocksTestStorage();
        });
    };

    ActionBlocker first_blocker;
    auto first = make_storage(first_blocker);
    manager.add(first, ActionLocks::PartsMerge);
    EXPECT_EQ(manager.getStoppedActions(first), Names{"merges"});
    first.reset();
    EXPECT_TRUE(first_blocker.isCancelled());

    /// A new storage at the recycled address must not observe the previous owner's controls.
    auto second = make_storage(first_blocker);
    EXPECT_TRUE(manager.getStoppedActions(second).empty());

    /// A control registered by the new owner replaces the stale entry, not merges with it.
    manager.add(second, ActionLocks::PartsMove);
    EXPECT_EQ(manager.getStoppedActions(second), Names{"moves"});
    manager.remove(second, ActionLocks::PartsMove);
    second.reset();

    /// Destroying the blocker expires the previous owner's locks; the next sweep reclaims the entry.
    {
        ActionBlocker expiring_blocker = std::move(first_blocker);
    }
    manager.cleanExpired();

    ActionBlocker third_blocker;
    auto third = make_storage(third_blocker);
    manager.add(third, ActionLocks::ViewRefresh);
    EXPECT_EQ(manager.getStoppedActions(third), Names{"view_refresh"});
    third.reset();
    manager.cleanExpired();
    EXPECT_TRUE(manager.getStoppedActions(third).empty());
}

TEST(ActionLocksManager, ExpiredLocksAreNotReportedUntilSwept)
{
    ActionLocksManager manager(getContext().context);
    auto blocker = std::make_unique<ActionBlocker>();
    auto storage = std::make_shared<ActionLocksTestStorage>(*blocker);

    manager.add(storage, ActionLocks::PartsMerge);
    manager.add(storage, ActionLocks::ViewRefreshPause);
    EXPECT_EQ(manager.getStoppedActions(storage), (Names{"merges", "view_refresh_pause"}));

    /// Destroying the blocker expires the locks, but the entries are only swept later.
    blocker.reset();
    EXPECT_EQ(manager.getStoppedActions(storage), (Names{"merges", "view_refresh_pause"}));
    manager.cleanExpired();
    EXPECT_TRUE(manager.getStoppedActions(storage).empty());
}

}
