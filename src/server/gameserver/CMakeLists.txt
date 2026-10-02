# ============================================================================
# GameServer 模块
# ============================================================================

# 通用编译定义和包含目录
set(GAMESERVER_COMPILE_DEFS __GAME_SERVER__ __COMBAT__)
set(GAMESERVER_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/database
    ${CMAKE_CURRENT_SOURCE_DIR}/skill
    ${CMAKE_CURRENT_SOURCE_DIR}/item
    ${CMAKE_CURRENT_SOURCE_DIR}/war
    ${CMAKE_CURRENT_SOURCE_DIR}/couple
    ${CMAKE_CURRENT_SOURCE_DIR}/mission
    ${CMAKE_CURRENT_SOURCE_DIR}/ctf
    ${CMAKE_CURRENT_SOURCE_DIR}/quest
    ${CMAKE_CURRENT_SOURCE_DIR}/quest/luaScript
    ${CMAKE_CURRENT_SOURCE_DIR}/mofus
)

# ============================================================================
# 子目录库
# ============================================================================
add_subdirectory(skill)
add_subdirectory(item)
add_subdirectory(war)
add_subdirectory(couple)
add_subdirectory(mission)
add_subdirectory(ctf)
add_subdirectory(quest)
add_subdirectory(mofus)

# ============================================================================
# Monster 相关源文件
# ============================================================================
set(MONSTER_SOURCES
    Monster.cpp
    MonsterInfo.cpp
    MonsterManager.cpp
    MonsterAI.cpp
    Directive.cpp
    MonsterNameManager.cpp
    MonsterCounter.cpp
)

# ============================================================================
# Item 相关源文件（gameserver 目录下的）
# ============================================================================
set(ITEM_SOURCES
    ConcreteItem.cpp
    Item.cpp
    ItemInfo.cpp
    ItemUtil.cpp
    ItemFactoryManager.cpp
    ItemInfoManager.cpp
    ItemLoaderManager.cpp
    SlayerCorpse.cpp
    VampireCorpse.cpp
    OustersCorpse.cpp
    MonsterCorpse.cpp
)

# ============================================================================
# Guild 相关源文件
# ============================================================================
set(GUILD_SOURCES
    Party.cpp
    party/PartyInviteDecision.cpp
    Guild.cpp
    GuildManager.cpp
    GuildUnion.cpp
    guild/GuildJoinDecision.cpp
    guild/GuildUnionJoinOffer.cpp
    guild/GuildUnionRegistry.cpp
    guild/GuildUnionTeardown.cpp
)

# ============================================================================
# Event 相关源文件
# ============================================================================
set(EVENT_SOURCES
    Event.cpp
    EventManager.cpp
    EventShutdown.cpp
    EventRegeneration.cpp
    EventSave.cpp
    EventMorph.cpp
    EventResurrect.cpp
    EventReloadInfo.cpp
    EventTransport.cpp
    EventKick.cpp
    EventSystemMessage.cpp
    EventRefreshHolyLandPlayer.cpp
    EventHeadCount.cpp
)

# ============================================================================
# Effect 相关源文件
# ============================================================================
set(EFFECT_SOURCES
    Effect.cpp
    EffectManager.cpp
    EffectSchedule.cpp
    EffectLoaderManager.cpp
    EffectShutDown.cpp
    EffectHPRecovery.cpp
    EffectMPRecovery.cpp
    EffectAlignmentRecovery.cpp
    EffectEnemyErase.cpp
    EffectDecayCorpse.cpp
    EffectDecayItem.cpp
    EffectAftermath.cpp
    EffectComa.cpp
    EffectPrecedence.cpp
    EffectTransportItem.cpp
    EffectAddItem.cpp
    EffectRelicTable.cpp
    EffectHasSlayerRelic.cpp
    EffectHasVampireRelic.cpp
    EffectIncreaseAttr.cpp
    EffectDeleteItem.cpp
    EffectSlayerRelic.cpp
    EffectVampireRelic.cpp
    EffectRelicPosition.cpp
    EffectRelicLock.cpp
    EffectMasterLairPass.cpp
    EffectContinualGroundAttack.cpp
    EffectGhost.cpp
    EffectTransportCreature.cpp
    EffectGrandMasterSlayer.cpp
    EffectGrandMasterVampire.cpp
    EffectKillAftermath.cpp
    EffectHasBloodBible.cpp
    EffectShrineHoly.cpp
    EffectShrineGuard.cpp
    EffectShrineShield.cpp
    EffectTransportItemToCorpse.cpp
    EffectAddItemToCorpse.cpp
    EffectHasRelic.cpp
    EffectHasCastleSymbol.cpp
    EffectLoveChain.cpp
    EffectPKZoneRegen.cpp
    EffectPKZoneResurrection.cpp
    EffectGrandMasterOusters.cpp
    EffectTranslation.cpp
    EffectLoud.cpp
    EffectMute.cpp
    SimpleCreatureEffect.cpp
    EffectRefiniumTicket.cpp
    EffectFlagInsert.cpp
    EffectHasSweeper.cpp
    EffectKeepSweeper.cpp
    EffectRegenZone.cpp
    EffectTryingPosition.cpp
    EffectTrying.cpp
    EffectTryRegenZone.cpp
    EffectOnBridge.cpp
    EffectHasPet.cpp
    EffectPacketSend.cpp
    EffectDarknessForbidden.cpp
    EffectCastingTrap.cpp
    EffectWithWarning.cpp
    EffectKickOut.cpp
    EffectGDRLairClose.cpp
    EffectEventQuestReset.cpp
    EffectShareHP.cpp
    EffectCanEnterGDRLair.cpp
    EffectAutoTurret.cpp
    EffectTurretLaser.cpp
    EffectKillTimer.cpp
    EffectDragonEye.cpp
    EffectRegenerate.cpp
    EffectRecallMotorcycle.cpp
    EffectRideMotorcycle.cpp
    EffectDonation200501.cpp
    SimpleTileEffect.cpp
    EffectDeleteTile.cpp
)

# ============================================================================
# GQuest 相关源文件
# ============================================================================
set(GQUEST_SOURCES
    GQuestInfo.cpp
    GQuestManager.cpp
    GQuestElement.cpp
    GQuestStatus.cpp
    GQuestInventory.cpp
    GQuestLevelElement.cpp
    GQuestTimeElement.cpp
    GQuestGiveVampireExpElement.cpp
    GQuestBloodDrainElement.cpp
    GQuestSayNPCElement.cpp
    GQuestExecuteElement.cpp
    GQuestGiveQuestItemElement.cpp
    GQuestLoseQuestItemElement.cpp
    GQuestGiveItemElement.cpp
    GQuestTamePetElement.cpp
    GQuestRaceElement.cpp
    GQuestKilledElement.cpp
    GQuestGiveDomainExpElement.cpp
    GQuestGiveMoneyElement.cpp
    GQuestSkillLevelElement.cpp
    GQuestRideMotorcycleElement.cpp
    GQuestGiveOustersExpElement.cpp
    GQuestTouchWayPointElement.cpp
    GQuestHasQuestItemElement.cpp
    GQuestPartyDissectElement.cpp
    GQuestKillMonsterElement.cpp
    GQuestGiveEventQuestItemElement.cpp
    GQuestEventPartyElement.cpp
    GQuestEventPartyCrashElement.cpp
    GQuestCompleteQuestElement.cpp
    GQuestFastMoveElement.cpp
    GQuestIllegalWarpElement.cpp
    GQuestCheckPoint.cpp
    GQuestTravelElement.cpp
    GQuestORElement.cpp
    GQuestNOTElement.cpp
    GQuestRandomElement.cpp
    GQuestAdvancementClassLevelElement.cpp
    GQuestClearDynamicZoneElement.cpp
    GQuestAddEffectElement.cpp
    GQuestRemoveEffectElement.cpp
    GQuestSetEnterDynamicZoneElement.cpp
    GQuestEnterDynamicZoneElement.cpp
    GQuestStartOtherQuestElement.cpp
    GQuestGiveAdvancementClassExpElement.cpp
    GQuestAdvanceClassElement.cpp
    GQuestWarpElement.cpp
)


# ============================================================================
# 主要源文件
# ============================================================================
# Gameserver-side halves of wire classes (docs/RESTRUCTURING.md 2.4):
# member functions that convert live game objects (Item, PetItem, ...) into
# wire fields. Declared in the packet headers, defined here so the wire
# library never depends on game objects.
set(PACKETFILL_SOURCES
    packetfill/GCShopListFill.cpp
    packetfill/GCShopListMysteriousFill.cpp
    packetfill/GCStashListFill.cpp
    packetfill/PetInfoFill.cpp
)

# Packet handlers, moved out of src/Core (docs/RESTRUCTURING.md 2.4):
# the app owns which handler runs; the kernel keeps only the wire classes.
set(HANDLER_SOURCES
    handler/CGAbsorbSoulHandler.cpp
    handler/CGAcceptUnionHandler.cpp
    handler/CGAddGearToMouseHandler.cpp
    handler/CGAddInventoryToMouseHandler.cpp
    handler/CGAddItemToCodeSheetHandler.cpp
    handler/CGAddItemToItemHandler.cpp
    handler/CGAddMouseToGearHandler.cpp
    handler/CGAddMouseToInventoryHandler.cpp
    handler/CGAddMouseToQuickSlotHandler.cpp
    handler/CGAddMouseToZoneHandler.cpp
    handler/CGAddQuickSlotToMouseHandler.cpp
    handler/CGAddSMSAddressHandler.cpp
    handler/CGAddZoneToInventoryHandler.cpp
    handler/CGAddZoneToMouseHandler.cpp
    handler/CGAppointSubmasterHandler.cpp
    handler/CGAttackHandler.cpp
    handler/CGAuthKeyHandler.cpp
    handler/CGBloodDrainHandler.cpp
    handler/CGBuyStoreItemHandler.cpp
    handler/CGCastingSkillHandler.cpp
    handler/CGConnectHandler.cpp
    handler/CGConnectSetKeyHandler.cpp
    handler/CGCrashReportHandler.cpp
    handler/CGDeleteSMSAddressHandler.cpp
    handler/CGDenyUnionHandler.cpp
    handler/CGDepositPetHandler.cpp
    handler/CGDisplayItemHandler.cpp
    handler/CGDissectionCorpseHandler.cpp
    handler/CGDonationMoneyHandler.cpp
    handler/CGDownSkillHandler.cpp
    handler/CGDropMoneyHandler.cpp
    handler/CGExchangeBuyHandler.cpp
    handler/CGExchangeListHandler.cpp
    handler/CGExpelGuildHandler.cpp
    handler/CGExpelGuildMemberHandler.cpp
    handler/CGFailQuestHandler.cpp
    handler/CGGQuestAcceptHandler.cpp
    handler/CGGQuestCancelHandler.cpp
    handler/CGGetEventItemHandler.cpp
    handler/CGGetOffMotorCycleHandler.cpp
    handler/CGGlobalChatHandler.cpp
    handler/CGGuildChatHandler.cpp
    handler/CGJoinGuildHandler.cpp
    handler/CGLearnSkillHandler.cpp
    handler/CGLogoutHandler.cpp
    handler/CGLotterySelectHandler.cpp
    handler/CGMakeItemHandler.cpp
    handler/CGMixItemHandler.cpp
    handler/CGModifyGuildIntroHandler.cpp
    handler/CGModifyGuildMemberHandler.cpp
    handler/CGModifyGuildMemberIntroHandler.cpp
    handler/CGModifyNicknameHandler.cpp
    handler/CGModifyTaxRatioHandler.cpp
    handler/CGMouseToStashHandler.cpp
    handler/CGMoveHandler.cpp
    handler/CGNPCAskAnswerHandler.cpp
    handler/CGNPCTalkHandler.cpp
    handler/CGPartyInviteHandler.cpp
    handler/CGPartyLeaveHandler.cpp
    handler/CGPartyPositionHandler.cpp
    handler/CGPartySayHandler.cpp
    handler/CGPetGambleHandler.cpp
    handler/CGPickupMoneyHandler.cpp
    handler/CGPortCheckHandler.cpp
    handler/CGQuitGuildHandler.cpp
    handler/CGQuitUnionAcceptHandler.cpp
    handler/CGQuitUnionDenyHandler.cpp
    handler/CGQuitUnionHandler.cpp
    handler/CGRangerSayHandler.cpp
    handler/CGReadyHandler.cpp
    handler/CGRegistGuildHandler.cpp
    handler/CGRelicToObjectHandler.cpp
    handler/CGReloadFromInventoryHandler.cpp
    handler/CGReloadFromQuickSlotHandler.cpp
    handler/CGRequestGuildListHandler.cpp
    handler/CGRequestGuildMemberListHandler.cpp
    handler/CGRequestIPHandler.cpp
    handler/CGRequestInfoHandler.cpp
    handler/CGRequestNewbieItemHandler.cpp
    handler/CGRequestPowerPointHandler.cpp
    handler/CGRequestRepairHandler.cpp
    handler/CGRequestStoreInfoHandler.cpp
    handler/CGRequestUnionHandler.cpp
    handler/CGRequestUnionInfoHandler.cpp
    handler/CGResurrectHandler.cpp
    handler/CGRideMotorCycleHandler.cpp
    handler/CGSMSAddressListHandler.cpp
    handler/CGSMSSendHandler.cpp
    handler/CGSayHandler.cpp
    handler/CGSelectBloodBibleHandler.cpp
    handler/CGSelectGuildHandler.cpp
    handler/CGSelectGuildMemberHandler.cpp
    handler/CGSelectNicknameHandler.cpp
    handler/CGSelectPortalHandler.cpp
    handler/CGSelectQuestHandler.cpp
    handler/CGSelectRankBonusHandler.cpp
    handler/CGSelectRegenZoneHandler.cpp
    handler/CGSelectTileEffectHandler.cpp
    handler/CGSelectWayPointHandler.cpp
    handler/CGSetSlayerHotKeyHandler.cpp
    handler/CGSetVampireHotKeyHandler.cpp
    handler/CGShopRequestBuyHandler.cpp
    handler/CGShopRequestListHandler.cpp
    handler/CGShopRequestSellHandler.cpp
    handler/CGSilverCoatingHandler.cpp
    handler/CGSkillToInventoryHandler.cpp
    handler/CGSkillToNamedHandler.cpp
    handler/CGSkillToObjectHandler.cpp
    handler/CGSkillToSelfHandler.cpp
    handler/CGSkillToTileHandler.cpp
    handler/CGStashDepositHandler.cpp
    handler/CGStashListHandler.cpp
    handler/CGStashRequestBuyHandler.cpp
    handler/CGStashToMouseHandler.cpp
    handler/CGStashWithdrawHandler.cpp
    handler/CGStoreCloseHandler.cpp
    handler/CGStoreOpenHandler.cpp
    handler/CGStoreSignHandler.cpp
    handler/CGSubmitScoreHandler.cpp
    handler/CGTakeOutGoodHandler.cpp
    handler/CGTameMonsterHandler.cpp
    handler/CGThrowBombHandler.cpp
    handler/CGThrowItemHandler.cpp
    handler/CGTradeAddItemHandler.cpp
    handler/CGTradeFinishHandler.cpp
    handler/CGTradeMoneyHandler.cpp
    handler/CGTradePrepareHandler.cpp
    handler/CGTradeRemoveItemHandler.cpp
    handler/CGTryJoinGuildHandler.cpp
    handler/CGTypeStringListHandler.cpp
    handler/CGUnburrowHandler.cpp
    handler/CGUndisplayItemHandler.cpp
    handler/CGUntransformHandler.cpp
    handler/CGUseBonusPointHandler.cpp
    handler/CGUseItemFromGQuestInventoryHandler.cpp
    handler/CGUseItemFromGearHandler.cpp
    handler/CGUseItemFromInventoryHandler.cpp
    handler/CGUseMessageItemFromInventoryHandler.cpp
    handler/CGUsePotionFromInventoryHandler.cpp
    handler/CGUsePotionFromQuickSlotHandler.cpp
    handler/CGUsePowerPointHandler.cpp
    handler/CGVerifyTimeHandler.cpp
    handler/CGVisibleHandler.cpp
    handler/CGWhisperHandler.cpp
    handler/CGWithdrawPetHandler.cpp
    handler/CGWithdrawTaxHandler.cpp
    handler/GCFriendChattingHandler.cpp
    handler/GGCommandHandler.cpp
    handler/GGGuildChatHandler.cpp
    handler/GGServerChatHandler.cpp
    handler/LGIncomingConnectionErrorHandler.cpp
    handler/LGIncomingConnectionHandler.cpp
    handler/LGIncomingConnectionOKHandler.cpp
    handler/LGKickCharacterHandler.cpp
    handler/SGAddGuildMemberOKHandler.cpp
    handler/SGAddGuildOKHandler.cpp
    handler/SGDeleteGuildOKHandler.cpp
    handler/SGExpelGuildMemberOKHandler.cpp
    handler/SGGuildInfoHandler.cpp
    handler/SGGuildMemberLogOnOKHandler.cpp
    handler/SGModifyGuildIntroOKHandler.cpp
    handler/SGModifyGuildMemberOKHandler.cpp
    handler/SGModifyGuildOKHandler.cpp
    handler/SGQuitGuildOKHandler.cpp
)

set(GAMESERVER_RUNTIME_SOURCES
    GamePacketDispatch.cpp
    Zone.cpp
    ZoneBroadcast.cpp
    ZoneItem.cpp
    ZoneLoad.cpp
    ZoneMove.cpp
    ZoneScan.cpp
    ZoneSpawn.cpp
    Slayer.cpp
    Vampire.cpp
    Ousters.cpp
    OustersEXPInfo.cpp
    ClientManager.cpp
    ConnectionInfoManager.cpp
    Creature.cpp
    CreatureManager.cpp
    DarkLightInfo.cpp
    Corpse.cpp
    GameContext.cpp
    GameFrameGate.cpp
    GamePlayer.cpp
    SpeedHackDecision.cpp
    GameServer.cpp
    IncomingPlayerManager.cpp
    SharedServerClient.cpp
    InfoClassManager.cpp
    Inventory.cpp
    LoginServerManager.cpp
    SharedServerManager.cpp
    NPC.cpp
    NPCManager.cpp
    ResurrectLocationManager.cpp
    PCFinder.cpp
    PlayerMailbox.cpp
    TelephoneCenter.cpp
    ParkingCenter.cpp
    PCManager.cpp
    Portal.cpp
    Script.cpp
    ScriptManager.cpp
    ThreadManager.cpp
    ThreadPool.cpp
    Tile.cpp
    Sector.cpp
    TimeManager.cpp
    GoodsInventory.cpp
    GoodsInfoManager.cpp
    VisionInfo.cpp
    WeatherInfo.cpp
    WeatherManager.cpp
    ZoneInfo.cpp
    ZoneGroup.cpp
    ZoneGroupManager.cpp
    ZoneInfoManager.cpp
    ZonePlayerManager.cpp
    ZoneGroupThread.cpp
    ObjectManager.cpp
    AbilityBalance.cpp
    PlayerCreature.cpp
    ZoneUtil.cpp
    Treasure.cpp
    PacketUtil.cpp
    ConnectionInfo.cpp
    InventorySlot.cpp
    ObjectRegistry.cpp
    WayPoint.cpp
    OptionInfo.cpp
    VolumeInfo.cpp
    VampEXPInfo.cpp
    ItemRack.cpp
    ShopRack.cpp
    Stash.cpp
    Garbage.cpp
    ShopTemplate.cpp
    PriceManager.cpp
    CreatureUtil.cpp
    ItemMap.cpp
    TradeManager.cpp
    trade/TradePrepareDecision.cpp
    trade/TradeTableContext.cpp
    trade/TradeTableDecision.cpp
    FlagSet.cpp
    AlignmentManager.cpp
    SkillInfo.cpp
    SkillDomainInfoManager.cpp
    SkillParentInfo.cpp
    InitAllStat.cpp
    SlayerStat.cpp
    VampireStat.cpp
    OustersStat.cpp
    PrecedenceTable.cpp
    VariableManager.cpp
    CombatInfoManager.cpp
    UniqueItemManager.cpp
    MasterLairInfoManager.cpp
    MasterLairManager.cpp
    MonsterSummonInfo.cpp
    LogNameManager.cpp
    RankBonusInfo.cpp
    RankBonus.cpp
    CastleInfoManager.cpp
    HolyLandRaceBonus.cpp
    ShrineInfoManager.cpp
    GlobalItemPositionLoader.cpp
    RelicUtil.cpp
    HolyLandManager.cpp
    BloodBibleBonus.cpp
    BloodBibleBonusManager.cpp
    CastleShrineInfoManager.cpp
    SkillPropertyManager.cpp
    StringPool.cpp
    PKZoneInfoManager.cpp
    GameServerGroupInfoManager.cpp
    DefaultOptionSetInfo.cpp
    CastleSkillInfo.cpp
    TimeLimitItemManager.cpp
    ItemMineInfo.cpp
    EventItemUtil.cpp
    SweeperSet.cpp
    SweeperBonus.cpp
    SweeperBonusManager.cpp
    LevelWar.cpp
    LevelWarManager.cpp
    LevelWarZoneInfoManager.cpp
    RegenZoneManager.cpp
    PetTypeInfo.cpp
    PetUtil.cpp
    PetAttrInfo.cpp
    PetExpInfo.cpp
    RankExpTable.cpp
    SlayerAttrExpTable.cpp
    Pet.cpp
    SystemAvailabilitiesManager.cpp
    ItemGradeManager.cpp
    EventZoneInfo.cpp
    SMSServiceThread.cpp
    SMSAddressBook.cpp
    FiniteStateMachine.cpp
    GDRLairManager.cpp
    GDRLairAbstractStates.cpp
    NicknameBook.cpp
    repository/MySQLNicknameRepository.cpp
    repository/MySQLPlayRecordRepository.cpp
    repository/MySQLBalanceInfoRepository.cpp
    repository/MySQLBloodBibleSignRepository.cpp
    repository/MySQLBulletinBoardRepository.cpp
    repository/MySQLComebackEventRepository.cpp
    repository/MySQLContentInfoRepository.cpp
    repository/MySQLCoupleRepository.cpp
    repository/MySQLFlagWarRepository.cpp
    repository/MySQLFriendRepository.cpp
    repository/MySQLMessageRepository.cpp
    repository/MySQLMofusPointRepository.cpp
    repository/MySQLRegenZoneRepository.cpp
    repository/MySQLSystemAvailabilityRepository.cpp
    repository/MySQLZoneInfoRepository.cpp
    repository/MySQLCharacterRepository.cpp
    repository/MySQLCharacterPurgeRepository.cpp
    repository/MySQLGoldRepository.cpp
    repository/MySQLGoodsRepository.cpp
    repository/MySQLGuildRepository.cpp
    repository/MySQLItemObjectRepository.cpp
    repository/MySQLItemRepository.cpp
    repository/MySQLEffectSaveRepository.cpp
    repository/MySQLFlagSetRepository.cpp
    repository/MySQLGameInfoRepository.cpp
    repository/MySQLQuestInfoRepository.cpp
    repository/MySQLQuestItemRepository.cpp
    repository/MySQLRankBonusRepository.cpp
    repository/MySQLSMSAddressRepository.cpp
    repository/MySQLSMSMessageRepository.cpp
    repository/MySQLSpecialEventRepository.cpp
    repository/MySQLSessionRepository.cpp
    repository/MySQLSkillSaveRepository.cpp
    repository/MySQLStashRepository.cpp
    repository/MySQLWarInfoRepository.cpp
    LevelNickInfoManager.cpp
    SiegeManager.cpp
    BroadcastFilter.cpp
    Store.cpp
    AdvancementClassExpTable.cpp
    DynamicZoneManager.cpp
    DynamicZoneInfo.cpp
    DynamicZone.cpp
    DynamicZoneGroup.cpp
    DynamicZoneGateOfAlter.cpp
    DynamicZoneAlterOfBlood.cpp
    DynamicZoneFactoryManager.cpp
    DynamicZoneSlayerMirrorOfAbyss.cpp
    DynamicZoneVampireMirrorOfAbyss.cpp
    DynamicZoneOustersMirrorOfAbyss.cpp
    NewYear2005ItemUtil.cpp
)

# ============================================================================
# Exchange System source files
# ============================================================================
set(EXCHANGE_SOURCES
    exchange/ExchangeDecision.cpp
    exchange/ExchangePurchase.cpp
    exchange/ExchangeService.cpp
    repository/MySQLExchangeRepository.cpp
)

# ============================================================================
# GM command sources
# ============================================================================
set(GM_SOURCES
    gm/CommandRouter.cpp
    gm/CommandRegistration.cpp
    gm/SubcommandTable.cpp
    gm/ConsoleCommandRegistration.cpp
    gm/RelayCommandRegistration.cpp
    gm/ServerCommands.cpp
    gm/PlayerCommands.cpp
    gm/ZoneCommands.cpp
    gm/ItemCommands.cpp
    gm/GuildCommands.cpp
    gm/ConsoleCommands.cpp
)

# ============================================================================
# GameServer runtime and executable
# ============================================================================
# Keep the implementation linkable without main(), which installs process
# handlers, starts the server and calls _Exit(). An OBJECT library preserves
# the existing link's placement of all game objects before the subsystem
# archives; those archives also call back into the game objects.
add_library(GameServerRuntime OBJECT
    ${PACKETFILL_SOURCES}
    ${HANDLER_SOURCES}
    ${GAMESERVER_RUNTIME_SOURCES}
    ${MONSTER_SOURCES}
    ${ITEM_SOURCES}
    ${GUILD_SOURCES}
    ${EFFECT_SOURCES}
    ${EVENT_SOURCES}
    ${GQUEST_SOURCES}
    ${EXCHANGE_SOURCES}
    ${GM_SOURCES}
)

# 编译定义
target_compile_definitions(GameServerRuntime PUBLIC ${GAMESERVER_COMPILE_DEFS})

# 包含目录
target_include_directories(GameServerRuntime PUBLIC ${GAMESERVER_INCLUDE_DIRS})

# 链接库（注意顺序，被依赖的库放后面）
if(APPLE)
    set(EXTRA_LIBS util z)
elseif(UNIX)
    set(EXTRA_LIBS nsl util dl z)
endif()

target_link_libraries(GameServerRuntime PUBLIC
    Items
    Quest
    Mofus
    GameServerDatabase
    ServerCore
    ServerWorkerShutdown
    ListenerStartup
    ServerProcessEnvironment
    GameServerPackets
    Skill
    Core
    LuaScript
    War
    Couple
    Mission
    CTF
    de-core
    # The wire archive itself (task 2.4) — also reached transitively through
    # Core/GameServerPackets, named here so the layering is visible.
    de-kernel
    ${MYSQL_LIBRARY}
    ${LUA51_LIBRARIES}
    Threads::Threads
    ${EXTRA_LIBS}
)

add_executable(gameserver main.cpp)
target_link_libraries(gameserver PRIVATE
    GameServerRuntime ServerApplication ServerProcessShutdown ServerFatalHandlers)

# 导出符号（用于动态加载）
set_target_properties(gameserver PROPERTIES ENABLE_EXPORTS ON)
