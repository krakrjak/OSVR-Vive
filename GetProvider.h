/** @file
    @brief Header

    @date 2016

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2016 Razer Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef INCLUDED_GetProvider_h_GUID_299C10C7_DE4D_4DB0_14DB_5CCAC70F3C11
#define INCLUDED_GetProvider_h_GUID_299C10C7_DE4D_4DB0_14DB_5CCAC70F3C11

// Internal Includes
#include "DriverLoader.h"
#include "InterfaceTraits.h"
#include "ServerDriverHost.h"

// Library/third-party includes
#include <openvr_driver.h>

// Standard includes
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace osvr {
namespace vive {

    template <typename InterfaceType>
    using ProviderPtr = std::shared_ptr<InterfaceType>;
    using SharedDriverLoader = std::shared_ptr<DriverLoader>;
    namespace detail {
        /// Implementation of getProvider, with an optional hook where you can
        /// do something else with the SharedDriverLoader before direct
        /// access to it is lost forever.
        template <typename InterfaceType, typename F>
        inline ProviderPtr<InterfaceType>
        getProviderImpl(std::unique_ptr<DriverLoader> &&loader,
                        vr::IVRDriverContext *context,
                        F &&driverLoaderFunctor) {
            using return_type = ProviderPtr<InterfaceType>;

            if (!loader) {
                return return_type{};
            }
            /// Move into local pointer, so if something goes wrong, the driver
            /// gets unloaded.
            std::unique_ptr<DriverLoader> myLoader(std::move(loader));
            auto rawPtr = myLoader->getInterfaceThrowing<InterfaceType>();
            auto initResults = rawPtr->Init(context);
            if (vr::VRInitError_None != initResults) {
                /// Failed, reset the loader pointer to unload the driver.
                std::cout << "Got error code " << initResults << std::endl;
                myLoader.reset();
                return return_type{};
            }

            /// OK, so this is the interface. Move the loader into a shared
            /// pointer, make the loader responsible for cleanup of the
            /// interface, and get the shared pointer of the interface pointer
            /// returned.
            SharedDriverLoader sharedLoader(std::move(myLoader));
            sharedLoader->cleanupInterfaceOnDestruction(rawPtr);

            /// Call the hook
            (std::forward<F>(driverLoaderFunctor))(sharedLoader);

            /// Create the return value: another shared pointer.
            /// This is the so-called "aliasing" constructor - this pointer will
            /// actually keep the DriverLoader alive and do nothing with the
            /// lifetime of rawPtr (which is why the loader is responsible for
            /// calling Cleanup, see above)
            return_type ret(sharedLoader, rawPtr);

            return ret;
        }
    } // namespace detail

    /// Gets one of the main interfaces provided by the driver entry point.
    /// Ownership of the driver loader is transferred (and maintained in the
    /// background by the returned interface smart pointer), so std::move that
    /// thing in.
    template <typename InterfaceType>
    inline ProviderPtr<InterfaceType>
    getProvider(std::unique_ptr<DriverLoader> &&loader,
                vr::IVRDriverContext *context) {
        static_assert(
            InterfaceExpectedFromEntryPointTrait<InterfaceType>::value,
            "Function only valid for those 'provider' interface types "
            "expected to be provided by the driver entry point.");
        return detail::getProviderImpl<InterfaceType>(
            std::move(loader), context, [](SharedDriverLoader const &) {});
    }

} // namespace vive
} // namespace osvr

#endif // INCLUDED_GetProvider_h_GUID_299C10C7_DE4D_4DB0_14DB_5CCAC70F3C11
